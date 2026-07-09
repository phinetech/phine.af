/**
 * @file af_orchestrator.cpp
 * @brief Implementation of the main AF orchestrator
 */

#include "af_orchestrator.h"
#include "events/event_dispatcher.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "common/communication/include/communication_factory.h"

namespace {

std::unordered_map<std::string, std::string> make_server_config(
    const af::config::CommunicationConfig& config) {
    std::unordered_map<std::string, std::string> result = {
        {"server_address", config.listen.host},
        {"server_port", af::config::port_to_string(config.listen.port)}
    };
    if (config.kind == af::config::CommunicationKind::Http) {
        result["base_path"] = config.base_path;
        result["use_tls"] = config.use_tls ? "true" : "false";
        result["timeout_ms"] = std::to_string(config.timeout_ms);
    }
    return result;
}

std::unordered_map<std::string, std::string> make_client_config(
    const af::config::CommunicationConfig& config,
    const af::config::EndpointConfig& endpoint) {
    std::unordered_map<std::string, std::string> result = {
        {"client_only", "true"},
        {"server_address", endpoint.host},
        {"server_port", af::config::port_to_string(endpoint.port)}
    };
    if (config.kind == af::config::CommunicationKind::Http) {
        result["base_url"] = "http" + std::string(config.use_tls ? "s" : "") + "://" + endpoint.host + ":" + af::config::port_to_string(endpoint.port);
        result["base_path"] = config.base_path;
        result["use_tls"] = config.use_tls ? "true" : "false";
        result["timeout_ms"] = std::to_string(config.timeout_ms);
    }
    return result;
}

} // namespace

namespace af {
namespace core {

// Inner class for handling messages
class AfOrchestrator::OrchestratorMessageHandler : public af::communication::MessageHandler {
public:
    OrchestratorMessageHandler(AfOrchestrator* orchestrator)
        : orchestrator_(orchestrator) {}

    af::communication::MessagePtr handle_message(
        const af::communication::MessagePtr& message) override {
        return orchestrator_->process_message(message);
    }

private:
    AfOrchestrator* orchestrator_;
};

AfOrchestrator::AfOrchestrator(const std::string& config_path)
    : AfComponent("af_orchestrator"), config_path_(config_path) {
    // Setup logger
    initializeLogger(spdlog::level::debug);

    logger_->info("Initializing AF Core Orchestrator");

    // Create components
    event_dispatcher_ = std::make_shared<af::core::events::EventDispatcher>();
    ue_state_manager_ = std::make_shared<UeStateManager>(event_dispatcher_);
    request_router_ = std::make_shared<RequestRouter>();
    policy_manager_ = std::make_shared<PolicyManager>(ue_state_manager_);
    subscription_manager_ = std::make_shared<SubscriptionManager>(ue_state_manager_);


    // Create message handler
    message_handler_ = std::make_shared<OrchestratorMessageHandler>(this);
}

AfOrchestrator::~AfOrchestrator() {
    stop();
}

void AfOrchestrator::initialize() {
    logger_->info("Initializing AF Core components");

    // Load configuration
    load_config();

    // Create QoD components after configuration is available
    initialize_qod_components();

    // Initialize components
    request_router_->initialize(this);
    policy_manager_->initialize(this);
    subscription_manager_->initialize(this);

    // Initialize communication interfaces first (needed for QoD components)
    initialize_communication();

    // Initialize QoD components (after communication is ready)
    if (communication_services_.find("pcf") != communication_services_.end()) {
        qod_session_manager_->initialize(communication_services_["pcf"], pcf_destination_);
    } else {
        logger_->warn("PCF service not available, QoD session manager initialized without PCF");
        qod_session_manager_->initialize(nullptr, "");
    }
    qod_notification_manager_->initialize(this);

    // Configure components
    configure_components();

    // Register message handlers
    register_handlers();

    // Register QoD-specific handlers
    register_qod_handlers();

    logger_->info("AF Core initialization complete");
}

void AfOrchestrator::load_config() {
    try {
        logger_->info("Loading configuration from {}", config_path_);
        app_config_ = af::config::load_app_config(config_path_);
        af::config::validate(app_config_);
    }
    catch (const std::exception& e) {
        logger_->error("Failed to load configuration: {}", e.what());
    }

    logger_->set_level(app_config_.af_core.logging.level);

    const auto& core_comm = app_config_.af_core.communication;
    const auto& pcf_comm = app_config_.pcf_handler.communication;

    if (app_config_.pcf_handler.enabled) {
        // af_core is the *client* of the PCF handler, so it must dial the
        // handler's address (remote), not its own bind address (listen).
        // For direct/bundled comm, remote is absent and resolve_destination
        // ignores the endpoint anyway, returning the service name.
        const auto& pcf_endpoint = pcf_comm.remote.value_or(pcf_comm.listen);
        pcf_destination_ = af::config::resolve_destination(
            pcf_comm.kind,
            pcf_endpoint,
            "pcf_handler");
    } else {
        pcf_destination_.clear();
    }

    logger_->info("AF Core communication kind: {} on {}",
                  af::config::to_string(core_comm.kind),
                  af::config::endpoint_to_string(core_comm.listen));
    if (app_config_.pcf_handler.enabled) {
        logger_->info("PCF handler communication kind: {} destination: {}",
                      af::config::to_string(pcf_comm.kind),
                      pcf_destination_);
    } else {
        logger_->info("PCF handler disabled in configuration");
    }
    logger_->info("Configuration loaded successfully");
}

void AfOrchestrator::initialize_communication() {
    try {
        logger_->info("Initializing communication interfaces");

        const auto& core_comm = app_config_.af_core.communication;
        const auto& pcf_comm = app_config_.pcf_handler.communication;

        // Create main communication service for inbound communication
        auto main_comm = af::communication::CommunicationFactory::create_service(
            af::config::to_string(core_comm.kind),
            "af_core",
            make_server_config(core_comm));

        if (!main_comm) {
            throw std::runtime_error("Failed to create main communication service");
        }

        communication_services_["main"] = main_comm;

        // Initialize communication with various southbound interfaces
        // (These would be more specific in a complete implementation)

        if (app_config_.pcf_handler.enabled) {
            // PCF interface
            std::unordered_map<std::string, std::string> pcf_comm_config;
            if (pcf_comm.kind == af::config::CommunicationKind::Grpc ||
                pcf_comm.kind == af::config::CommunicationKind::Http) {
                // Connect to the handler's address (remote), not af_core's own
                // listen address. validate() guarantees remote is set here;
                // value_or keeps us safe since load_config does not rethrow.
                const auto& pcf_endpoint = pcf_comm.remote.value_or(pcf_comm.listen);
                pcf_comm_config = make_client_config(pcf_comm, pcf_endpoint);
            }

            auto pcf_service = af::communication::CommunicationFactory::create_service(
                af::config::to_string(pcf_comm.kind),
                "af_core_pcf",
                pcf_comm_config);

            if (pcf_service) {
                communication_services_["pcf"] = pcf_service;
            }
        }

        // Initialize REST endpoints if using HTTP transport
        if (core_comm.kind == af::config::CommunicationKind::Http) {
            initialize_rest_endpoints();
        }

        logger_->info("Communication interfaces initialized");
    }
    catch (const std::exception& e) {
        logger_->error("Failed to initialize communication: {}", e.what());
        throw;
    }
}

void AfOrchestrator::register_handlers() {
    logger_->info("Registering message handlers");

    // Register handlers for the main communication service
    auto main_comm = communication_services_["main"];
    if (main_comm) {
        // Register generic message handler
        main_comm->register_handler("*", message_handler_);

        // Register handler for health check messages that return "OK"
        // Handler must be a MessageHandlerPtr, so wrap the lambda in a MessageHandler implementation
        class HealthCheckHandler : public af::communication::MessageHandler {
        public:
            af::communication::MessagePtr handle_message(const af::communication::MessagePtr& msg) override {
                auto response = std::make_shared<af::communication::Message>();
                response->message_type = "health_check_response";
                response->correlation_id = msg->correlation_id;
                std::string payload_str = "OK";
                response->payload = std::vector<uint8_t>(payload_str.begin(), payload_str.end());
                return response;
            }
        };
        static std::shared_ptr<af::communication::MessageHandler> health_check_handler = std::make_shared<HealthCheckHandler>();
        main_comm->register_handler("health_check", health_check_handler);

        // Register specific message type handlers
        main_comm->register_handler("qos_request", message_handler_);
        main_comm->register_handler("get_subscriptions", message_handler_);
        main_comm->register_handler("create_subscription", message_handler_);
        main_comm->register_handler("get_subscription", message_handler_);
        main_comm->register_handler("delete_subscription", message_handler_);

        // Register QoD message handlers
        main_comm->register_handler("qod_create_session", message_handler_);
        main_comm->register_handler("qod_get_session", message_handler_);
        main_comm->register_handler("qod_delete_session", message_handler_);
        main_comm->register_handler("qod_extend_session", message_handler_);
        main_comm->register_handler("qod_retrieve_sessions", message_handler_);

        logger_->info("Message handlers registered");
    }
}

void AfOrchestrator::configure_components() {
    logger_->info("Configuring AF Core components");

    // Configure the request router with handlers
    request_router_->register_handler("qos_request",
        [this](const af::communication::MessagePtr& msg) -> af::communication::MessagePtr {
            return policy_manager_->handle_qos_request(msg);
        });

    request_router_->register_handler("get_subscriptions",
        [this](const af::communication::MessagePtr& msg) -> af::communication::MessagePtr {
            return subscription_manager_->get_all_subscriptions(msg);
        });

    request_router_->register_handler("create_subscription",
        [this](const af::communication::MessagePtr& msg) -> af::communication::MessagePtr {
            return subscription_manager_->create_subscription(msg);
        });

    request_router_->register_handler("get_subscription",
        [this](const af::communication::MessagePtr& msg) -> af::communication::MessagePtr {
            return subscription_manager_->get_subscription(msg);
        });

    request_router_->register_handler("delete_subscription",
        [this](const af::communication::MessagePtr& msg) -> af::communication::MessagePtr {
            return subscription_manager_->delete_subscription(msg);
        });

    // Configure policy manager
    // TODO: Apply policy configuration from loaded config

    // Configure subscription manager
    // TODO: Apply subscription configuration from loaded config

    logger_->info("Components configured");
}

void AfOrchestrator::initialize_qod_components() {
    logger_->info("Initializing QoD components");

    // Configure QoD session manager
    af::qod::QodSessionConfig qod_config;
    qod_config.max_session_duration = app_config_.af_core.qod.max_session_duration;
    qod_config.min_session_duration = app_config_.af_core.qod.min_session_duration;
    qod_config.session_cleanup_interval = app_config_.af_core.qod.session_cleanup_interval;
    qod_config.unavailable_session_ttl = app_config_.af_core.qod.unavailable_session_ttl;
    qod_config.enable_notifications = app_config_.af_core.qod.enable_notifications;
    qod_config.api_base_url = app_config_.af_core.qod.api_base_url;

    // Create QoD state manager
    qod_state_manager_ = std::make_shared<af::qod::QodStateManager>();
    // Create QoD session manager
    qod_session_manager_ = std::make_shared<af::qod::QodSessionManager>(
        ue_state_manager_, qod_state_manager_, qod_config);

    // Create QoD handler
    qod_handler_ = std::make_shared<af::qod::QodHandler>(qod_session_manager_);

    // Create QoD notification manager
    qod_notification_manager_ = std::make_shared<af::qod::QodNotificationManager>();

    // Wire up notification delivery to session manager
    qod_session_manager_->set_notification_handler(qod_notification_manager_);

    logger_->info("QoD components created");
}

void AfOrchestrator::register_qod_handlers() {
    logger_->info("Registering QoD message handlers");

    // Register CAMARA QoD API handlers
    request_router_->register_handler("qod_create_session",
        [this](const af::communication::MessagePtr& msg) {
            return qod_handler_->handle_create_session(msg);
        });

    request_router_->register_handler("qod_get_session",
        [this](const af::communication::MessagePtr& msg) {
            return qod_handler_->handle_get_session(msg);
        });

    request_router_->register_handler("qod_delete_session",
        [this](const af::communication::MessagePtr& msg) {
            return qod_handler_->handle_delete_session(msg);
        });

    request_router_->register_handler("qod_extend_session",
        [this](const af::communication::MessagePtr& msg) {
            return qod_handler_->handle_extend_session(msg);
        });

    request_router_->register_handler("qod_retrieve_sessions",
        [this](const af::communication::MessagePtr& msg) {
            return qod_handler_->handle_retrieve_sessions(msg);
        });

    logger_->info("QoD message handlers registered");
}

void AfOrchestrator::initialize_rest_endpoints() {
    logger_->info("Initializing REST endpoints for HTTP transport");

    // Create REST router
    rest_router_ = std::make_shared<rest::RestRouter>();

    // Create QoD REST adapter
    qod_rest_adapter_ = std::make_shared<qod::QodRestAdapter>(
        rest_router_,
        request_router_);

    // Register CAMARA QoD endpoints with the main HTTP service
    auto main_comm = communication_services_["main"];
    if (main_comm) {
        qod_rest_adapter_->register_endpoints(main_comm);
        logger_->info("REST endpoints registered successfully");
    } else {
        logger_->warn("Main communication service not available for REST endpoint registration");
    }
}

void AfOrchestrator::start() {
    {
        std::lock_guard<std::mutex> lock(wait_mutex_);
        if (isRunning()) {
            logger_->warn("AF Core services already running");
            return;
        }
    }

    logger_->info("Starting AF Core services");

    // Start all communication services
    for (const auto& [name, service] : communication_services_) {
        if (service) {
            logger_->info("Starting communication service: {}", name);
            if (!service->start()) {
                logger_->error("Failed to start communication service: {}", name);
            }
        }
    }

    // Start QoD session manager
    if (qod_session_manager_) {
        qod_session_manager_->start();
        logger_->info("Started QoD session manager");
    }

    // Start QoD notification manager
    if (qod_notification_manager_) {
        qod_notification_manager_->start();
        logger_->info("Started QoD notification manager");
    }

    // Start components
    // Note: Most components don't need explicit start/stop,
    // they just need to be initialized and will operate based on messages

    {
        std::lock_guard<std::mutex> lock(wait_mutex_);
        setRunning(true);
    }

    logger_->info("AF Core services started");
}

void AfOrchestrator::stop() {
    {
        std::lock_guard<std::mutex> lock(wait_mutex_);
        if (!isRunning()) {
            logger_->info("AF Core services already stopped");
            wait_cv_.notify_all();
            return;
        }
    }

    logger_->info("Stopping AF Core services");

    // Stop QoD components first
    if (qod_notification_manager_) {
        qod_notification_manager_->stop();
        logger_->info("Stopped QoD notification manager");
    }

    if (qod_session_manager_) {
        qod_session_manager_->stop();
        logger_->info("Stopped QoD session manager");
    }

    // Stop all communication services
    for (const auto& [name, service] : communication_services_) {
        if (service) {
            logger_->info("Stopping communication service: {}", name);
            service->stop();
        }
    }

    {
        std::lock_guard<std::mutex> lock(wait_mutex_);
        setRunning(false);
    }
    wait_cv_.notify_all();

    logger_->info("AF Core services stopped");
}

void AfOrchestrator::wait() {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    wait_cv_.wait(lock, [this]() {
        return !isRunning();
    });
}

af::communication::MessagePtr AfOrchestrator::process_message(
    const af::communication::MessagePtr& message) {

    if (!message) {
        logger_->error("Received null message");
        return nullptr;
    }

    logger_->debug("Processing message of type: {}", message->message_type);

    // Route the message to the appropriate handler
    return request_router_->route_message(message);
}

void AfOrchestrator::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("af_orchestrator");

    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("af_orchestrator");
    }

    // Set the log level
    logger_->set_level(log_level);

    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

} // namespace core
} // namespace af