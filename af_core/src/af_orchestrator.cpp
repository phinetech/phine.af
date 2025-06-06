/**
 * @file af_orchestrator.cpp
 * @brief Implementation of the main AF orchestrator
 */

#include "af_orchestrator.h"
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
    initializeLogger();
    
    logger_->info("Initializing AF Core Orchestrator");
    
    // Create components
    request_router_ = std::make_shared<RequestRouter>();
    policy_manager_ = std::make_shared<PolicyManager>();
    subscription_manager_ = std::make_shared<SubscriptionManager>();
    
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
    
    // Initialize communication interfaces
    initialize_communication();
    
    // Configure components
    configure_components();
    
    // Register message handlers
    register_handlers();
    
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
        
        // Register specific message type handlers
        main_comm->register_handler("qos_request", message_handler_);
        main_comm->register_handler("get_subscriptions", message_handler_);
        main_comm->register_handler("create_subscription", message_handler_);
        main_comm->register_handler("get_subscription", message_handler_);
        main_comm->register_handler("delete_subscription", message_handler_);
        
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
    
    // Start components
    // Note: Most components don't need explicit start/stop,
    // they just need to be initialized and will operate based on messages
    
    logger_->info("AF Core services started");
}

void AfOrchestrator::stop() {
    logger_->info("Stopping AF Core services");
    
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