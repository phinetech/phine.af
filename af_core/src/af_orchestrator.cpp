/**
 * @file af_orchestrator.cpp
 * @brief Implementation of the main AF orchestrator
 */

#include "af_orchestrator.h"
#include "events/event_dispatcher.h"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "../common/communication/include/communication_factory.h"

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


    // Create QoD components
    initialize_qod_components();

    // event_dispatcher_->subscribe<af::core::events::PduSessionTerminatedEvent>(
    //     [this](const auto& event) {
    //         logger_->info("Received PDU Session Terminated Event for SUPI: {}, PDU Session ID: {}",
    //                       event.supi.value, event.pdu_session_id);
    //         // Handle the event as needed
    //         qod_session_manager_->handle_pdu_session_terminated(event);
    //     });

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

    // Initialize components
    request_router_->initialize(this);
    policy_manager_->initialize(this);
    subscription_manager_->initialize(this);

    // Initialize communication interfaces first (needed for QoD components)
    initialize_communication();

    // Initialize QoD components (after communication is ready)
    if (communication_services_.find("pcf") != communication_services_.end()) {
        qod_session_manager_->initialize(communication_services_["pcf"]);
    } else {
        logger_->warn("PCF service not available, QoD session manager initialized without PCF");
        qod_session_manager_->initialize(nullptr);
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
        YAML::Node config = YAML::LoadFile(config_path_);

        // TODO: Load specific configuration values

        logger_->info("Configuration loaded successfully");
    }
    catch (const std::exception& e) {
        logger_->error("Failed to load configuration: {}", e.what());
        // Use default configuration
    }
}

void AfOrchestrator::initialize_communication() {
    try {
        logger_->info("Initializing communication interfaces");

        // Create main communication service for inbound communication
        std::unordered_map<std::string, std::string> main_comm_config;
        // TODO: Load specific configuration values for main communication service
        main_comm_config["server_address"] = "0.0.0.0";
        main_comm_config["server_port"] = "50051";  // Use a fixed port for the core

        auto main_comm = af::communication::CommunicationFactory::create_service(
            "grpc", "af_core", main_comm_config);

        if (!main_comm) {
            throw std::runtime_error("Failed to create main communication service");
        }

        communication_services_["main"] = main_comm;

        // Initialize communication with various southbound interfaces
        // (These would be more specific in a complete implementation)

        // PCF interface
        std::unordered_map<std::string, std::string> pcf_comm_config;
        pcf_comm_config["client_only"] = "true";
        pcf_comm_config["server_address"] = "192.168.70.140";  // Example address
        pcf_comm_config["server_port"] = "50052";  // Example port for PCF
        auto pcf_comm = af::communication::CommunicationFactory::create_service(
            "grpc", "af_core_pcf", pcf_comm_config);

        if (pcf_comm) {
            communication_services_["pcf"] = pcf_comm;
        }

        // NEF interface
        std::unordered_map<std::string, std::string> nef_comm_config;
        auto nef_comm = af::communication::CommunicationFactory::create_service(
            "grpc", "af_core_nef", nef_comm_config);

        if (nef_comm) {
            communication_services_["nef"] = nef_comm;
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
    qod::QodSessionConfig qod_config;
    qod_config.max_session_duration = std::chrono::seconds(86400); // 24 hours
    qod_config.min_session_duration = std::chrono::seconds(60);     // 1 minute
    qod_config.session_cleanup_interval = std::chrono::seconds(60);
    qod_config.unavailable_session_ttl = std::chrono::seconds(360);
    qod_config.enable_notifications = true;
    qod_config.api_base_url = "https://api.example.com/quality-on-demand/v1";

    // TODO: Load QoD configuration from config file
    // load_qod_config(qod_config);

    // Create QoD state manager
    qod_state_manager_ = std::make_shared<qod::QodStateManager>();
    // Create QoD session manager
    qod_session_manager_ = std::make_shared<qod::QodSessionManager>(
        ue_state_manager_, qod_state_manager_, qod_config);

    // Create QoD handler
    qod_handler_ = std::make_shared<qod::QodHandler>(qod_session_manager_);

    // Create QoD notification manager
    qod_notification_manager_ = std::make_shared<qod::QodNotificationManager>();

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

void AfOrchestrator::start() {
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

    logger_->info("AF Core services started");
}

void AfOrchestrator::stop() {
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

    logger_->info("AF Core services stopped");
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