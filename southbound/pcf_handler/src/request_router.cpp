/**
 * @file request_router.cpp
 * @brief Implementation of the request router
 */

#include "request_router.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "pcf_handler.h"

namespace af {
namespace southbound {

RequestRouter::RequestRouter() {
    // Setup logger
    initializeLogger(spdlog::level::debug);
    
    logger_->info("Request Router created");
    
    // Create default handler
    default_handler_ = create_default_handler();
}

RequestRouter::~RequestRouter() {
    // Clean up resources
}

void RequestRouter::initialize(PcfHandler* handler) {
    handler_ = handler;
    logger_->info("Request Router initialized");
}

void RequestRouter::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("af_pcf_router");
    
    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("af_pcf_router");
    }
    
    // Set the log level
    logger_->set_level(log_level);
    
    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void RequestRouter::register_handler(
    const std::string& message_type, 
    const MessageHandlerFunc& handler) {
    
    if (!handler) {
        logger_->error("Attempted to register null handler for message type: {}", message_type);
        return;
    }
    
    handlers_[message_type] = handler;
    logger_->info("Registered handler for message type: {}", message_type);
}

af::communication::MessagePtr RequestRouter::route_message(
    const af::communication::MessagePtr& message) {
    
    if (!message) {
        logger_->error("Attempted to route null message");
        return nullptr;
    }
    
    const std::string& message_type = message->message_type;
    logger_->debug("Routing message of type: {}", message_type);
    
    // Find handler for this message type
    auto it = handlers_.find(message_type);
    if (it != handlers_.end()) {
        logger_->debug("Found handler for message type: {}", message_type);
        return it->second(message);
    }
    
    // No specific handler found, try using default handler
    logger_->warn("No handler found for message type: {}", message_type);
    return default_handler_(message);
}

MessageHandlerFunc RequestRouter::create_default_handler() {
    return [this](const af::communication::MessagePtr& message) -> af::communication::MessagePtr {
        logger_->warn("Using default handler for message type: {}", message->message_type);
        
        // Create error response
        auto response = std::make_shared<af::communication::Message>();
        response->message_type = "error.unhandled_message_type";
        response->correlation_id = message->correlation_id;
        
        // Set error message in payload
        std::string error_msg = "No handler registered for message type: " + message->message_type;
        response->payload.assign(error_msg.begin(), error_msg.end());
        
        return response;
    };
}

} // namespace southbound
} // namespace af