/**
 * @file subscription_manager.cpp
 * @brief Implementation of the subscription manager
 */

#include "subscription_manager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <random>
#include <sstream>
#include "af_orchestrator.h"

namespace af {
namespace core {

SubscriptionManager::SubscriptionManager(std::shared_ptr<UeStateManager> ue_state_manager)
        : ue_state_manager_(ue_state_manager) {
    // Setup logger
    initializeLogger(spdlog::level::debug);
    
    logger_->info("Subscription Manager created");
}

SubscriptionManager::~SubscriptionManager() {
    // Clean up resources
}

void SubscriptionManager::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("af_subscription");
    
    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("af_subscription");
    }
    
    // Set the log level
    logger_->set_level(log_level);
    
    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void SubscriptionManager::initialize(AfOrchestrator* orchestrator) {
    orchestrator_ = orchestrator;
    logger_->info("Subscription Manager initialized");

    // Add a UE to the UE state manager for testing purposes
    // This is just for demonstration; in a real application, this would be done by the
    // UE registration process.
    if (ue_state_manager_) {
        AfUeSubscriptionState test_ue_state;
        test_ue_state.supi = Supi{"imsi-208950000000001"}; // Example SUPI
        test_ue_state.gpsi = Gpsi{"gpsi-208950000000001"}; // Example GPSI
        test_ue_state.location_info = UeLocationInfo{
            UserLocation{
                std::nullopt, // No 5G location area
                {Tac{"12345"}}, // Example TAI list
                std::nullopt // No geographical area
            },
            std::nullopt // No time zone specified
        };
        test_ue_state.location_timestamp = DateTime{"2023-10-01T12:00:00Z"}; // Example timestamp
        test_ue_state.pdu_sessions = {
            PduSessionData{
                "session-1", // Example PDU session ID
                Dnn{"internet"}, // Example DNN
                std::nullopt, // No S-NSSAI specified
                "IPV4", // Example PDU session type
                "ACTIVE", // Session status
                DateTime{"2023-10-01T12:00:00Z"}, // Status timestamp
                Ipv4Addr{"10.60.0.1"}, // Example IPv4 address
            }
        };
        ue_state_manager_->update_ue_state(test_ue_state);
    } else {
        logger_->warn("UE State Manager is not initialized, cannot add test UE");
    }
}

af::communication::MessagePtr SubscriptionManager::create_subscription(
    const af::communication::MessagePtr& message) {
    
    logger_->info("Creating new subscription");
    
    auto response = std::make_shared<af::communication::Message>();
    response->correlation_id = message->correlation_id;
    
    try {
        // Extract payload as JSON
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto subscription_data = nlohmann::json::parse(payload_str);
        
        // Create new subscription
        Subscription subscription;
        subscription.id = generate_subscription_id();
        
        // Extract subscription parameters
        if (subscription_data.contains("client_id")) {
            subscription.client_id = subscription_data["client_id"];
        } else {
            // Use source service as client ID if not specified
            subscription.client_id = message->metadata.count("source_service") ?
                message->metadata.at("source_service") : "unknown";
        }
        
        if (!subscription_data.contains("event_type")) {
            throw std::runtime_error("Missing required field: event_type");
        }
        subscription.event_type = subscription_data["event_type"];
        
        // For external notifications
        if (subscription_data.contains("notification_url")) {
            subscription.notification_url = subscription_data["notification_url"];
        }
        
        // For internal callbacks
        if (subscription_data.contains("callback_service")) {
            subscription.callback_service = subscription_data["callback_service"];
        }
        
        // Must have either notification_url or callback_service
        if (subscription.notification_url.empty() && subscription.callback_service.empty()) {
            throw std::runtime_error("Either notification_url or callback_service must be specified");
        }
        
        // Extract filter criteria
        if (subscription_data.contains("filter_criteria") && subscription_data["filter_criteria"].is_object()) {
            for (auto& [key, value] : subscription_data["filter_criteria"].items()) {
                subscription.filter_criteria[key] = value.is_string() ? 
                    value.get<std::string>() : value.dump();
            }
        }
        
        // Set expiry time
        int expiry_seconds = 3600;  // Default: 1 hour
        if (subscription_data.contains("expiry_seconds")) {
            expiry_seconds = subscription_data["expiry_seconds"];
        }
        
        subscription.expiry_time = std::chrono::system_clock::now() + 
                                  std::chrono::seconds(expiry_seconds);
        
        subscription.active = true;
        
        // Validate the subscription
        if (!validate_subscription(subscription)) {
            throw std::runtime_error("Invalid subscription configuration");
        }
        
        // Store the subscription
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_[subscription.id] = subscription;
        }
        
        logger_->info("Created subscription with ID: {}, event type: {}", 
                     subscription.id, subscription.event_type);
        
        // Create success response
        response->message_type = "subscription_created";
        
        // Convert subscription to JSON
        nlohmann::json result = {
            {"id", subscription.id},
            {"event_type", subscription.event_type},
            {"client_id", subscription.client_id}
        };
        
        if (!subscription.notification_url.empty()) {
            result["notification_url"] = subscription.notification_url;
        }
        
        if (!subscription.callback_service.empty()) {
            result["callback_service"] = subscription.callback_service;
        }
        
        if (!subscription.filter_criteria.empty()) {
            nlohmann::json filter;
            for (const auto& [key, value] : subscription.filter_criteria) {
                filter[key] = value;
            }
            result["filter_criteria"] = filter;
        }
        
        // Convert expiry time to ISO string
        auto expiry_time_t = std::chrono::system_clock::to_time_t(subscription.expiry_time);
        std::tm tm = *std::gmtime(&expiry_time_t);
        char expiry_str[30];
        std::strftime(expiry_str, sizeof(expiry_str), "%Y-%m-%dT%H:%M:%SZ", &tm);
        result["expiry_time"] = expiry_str;
        
        std::string result_str = result.dump();
        response->payload.assign(result_str.begin(), result_str.end());
    }
    catch (const std::exception& e) {
        logger_->error("Error creating subscription: {}", e.what());
        
        // Create error response
        response->message_type = "subscription_error";
        
        nlohmann::json error = {
            {"error", "bad_request"},
            {"message", e.what()}
        };
        
        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
    }
    
    return response;
}

af::communication::MessagePtr SubscriptionManager::get_all_subscriptions(
    const af::communication::MessagePtr& message) {
    
    logger_->info("Getting all subscriptions");
    
    auto response = std::make_shared<af::communication::Message>();
    response->message_type = "subscriptions_list";
    response->correlation_id = message->correlation_id;
    
    // Get client ID from request if available
    std::string client_id;
    std::string payload_str(message->payload.begin(), message->payload.end());
    
    if (!payload_str.empty()) {
        try {
            auto request_data = nlohmann::json::parse(payload_str);
            if (request_data.contains("client_id")) {
                client_id = request_data["client_id"];
            }
        }
        catch (const std::exception& e) {
            logger_->warn("Error parsing get_all_subscriptions payload: {}", e.what());
        }
    }
    
    // If no client ID in request, try to get from message metadata
    if (client_id.empty() && message->metadata.count("source_service")) {
        client_id = message->metadata.at("source_service");
    }
    
    // Build response
    nlohmann::json subscriptions_json = nlohmann::json::array();
    
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        for (const auto& [id, subscription] : subscriptions_) {
            // Filter by client ID if specified
            if (!client_id.empty() && subscription.client_id != client_id) {
                continue;
            }
            
            nlohmann::json sub_json = {
                {"id", subscription.id},
                {"event_type", subscription.event_type},
                {"client_id", subscription.client_id},
                {"active", subscription.active}
            };
            
            if (!subscription.notification_url.empty()) {
                sub_json["notification_url"] = subscription.notification_url;
            }
            
            if (!subscription.callback_service.empty()) {
                sub_json["callback_service"] = subscription.callback_service;
            }
            
            // Convert expiry time to ISO string
            auto expiry_time_t = std::chrono::system_clock::to_time_t(subscription.expiry_time);
            std::tm tm = *std::gmtime(&expiry_time_t);
            char expiry_str[30];
            std::strftime(expiry_str, sizeof(expiry_str), "%Y-%m-%dT%H:%M:%SZ", &tm);
            sub_json["expiry_time"] = expiry_str;
            
            subscriptions_json.push_back(sub_json);
        }
    }
    
    std::string result_str = subscriptions_json.dump();
    response->payload.assign(result_str.begin(), result_str.end());
    
    return response;
}

af::communication::MessagePtr SubscriptionManager::get_subscription(
    const af::communication::MessagePtr& message) {
    
    auto response = std::make_shared<af::communication::Message>();
    response->correlation_id = message->correlation_id;
    
    try {
        // Extract subscription ID from payload
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto request_data = nlohmann::json::parse(payload_str);
        
        if (!request_data.contains("id")) {
            throw std::runtime_error("Missing required field: id");
        }
        
        std::string subscription_id = request_data["id"];
        logger_->info("Getting subscription: {}", subscription_id);
        
        // Get client ID for authorization check
        std::string client_id;
        if (request_data.contains("client_id")) {
            client_id = request_data["client_id"];
        } else if (message->metadata.count("source_service")) {
            client_id = message->metadata.at("source_service");
        }
        
        // Find the subscription
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        auto it = subscriptions_.find(subscription_id);
        
        if (it == subscriptions_.end()) {
            response->message_type = "subscription_not_found";
            
            nlohmann::json error = {
                {"error", "not_found"},
                {"message", "Subscription not found: " + subscription_id}
            };
            
            std::string error_str = error.dump();
            response->payload.assign(error_str.begin(), error_str.end());
            
            return response;
        }
        
        const Subscription& subscription = it->second;
        
        // If client ID is specified, check authorization
        if (!client_id.empty() && subscription.client_id != client_id) {
            response->message_type = "subscription_unauthorized";
            
            nlohmann::json error = {
                {"error", "unauthorized"},
                {"message", "Not authorized to access this subscription"}
            };
            
            std::string error_str = error.dump();
            response->payload.assign(error_str.begin(), error_str.end());
            
            return response;
        }
        
        // Create response with subscription details
        response->message_type = "subscription";
        
        nlohmann::json result = {
            {"id", subscription.id},
            {"event_type", subscription.event_type},
            {"client_id", subscription.client_id},
            {"active", subscription.active}
        };
        
        if (!subscription.notification_url.empty()) {
            result["notification_url"] = subscription.notification_url;
        }
        
        if (!subscription.callback_service.empty()) {
            result["callback_service"] = subscription.callback_service;
        }
        
        if (!subscription.filter_criteria.empty()) {
            nlohmann::json filter;
            for (const auto& [key, value] : subscription.filter_criteria) {
                filter[key] = value;
            }
            result["filter_criteria"] = filter;
        }
        
        // Convert expiry time to ISO string
        auto expiry_time_t = std::chrono::system_clock::to_time_t(subscription.expiry_time);
        std::tm tm = *std::gmtime(&expiry_time_t);
        char expiry_str[30];
        std::strftime(expiry_str, sizeof(expiry_str), "%Y-%m-%dT%H:%M:%SZ", &tm);
        result["expiry_time"] = expiry_str;
        
        std::string result_str = result.dump();
        response->payload.assign(result_str.begin(), result_str.end());
    }
    catch (const std::exception& e) {
        logger_->error("Error getting subscription: {}", e.what());
        
        response->message_type = "subscription_error";
        
        nlohmann::json error = {
            {"error", "bad_request"},
            {"message", e.what()}
        };
        
        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
    }
    
    return response;
}

af::communication::MessagePtr SubscriptionManager::delete_subscription(
    const af::communication::MessagePtr& message) {
    
    auto response = std::make_shared<af::communication::Message>();
    response->correlation_id = message->correlation_id;
    
    try {
        // Extract subscription ID from payload
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto request_data = nlohmann::json::parse(payload_str);
        
        if (!request_data.contains("id")) {
            throw std::runtime_error("Missing required field: id");
        }
        
        std::string subscription_id = request_data["id"];
        logger_->info("Deleting subscription: {}", subscription_id);
        
        // Get client ID for authorization check
        std::string client_id;
        if (request_data.contains("client_id")) {
            client_id = request_data["client_id"];
        } else if (message->metadata.count("source_service")) {
            client_id = message->metadata.at("source_service");
        }
        
        // Find and delete the subscription
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        auto it = subscriptions_.find(subscription_id);
        
        if (it == subscriptions_.end()) {
            response->message_type = "subscription_not_found";
            
            nlohmann::json error = {
                {"error", "not_found"},
                {"message", "Subscription not found: " + subscription_id}
            };
            
            std::string error_str = error.dump();
            response->payload.assign(error_str.begin(), error_str.end());
            
            return response;
        }
        
        const Subscription& subscription = it->second;
        
        // If client ID is specified, check authorization
        if (!client_id.empty() && subscription.client_id != client_id) {
            response->message_type = "subscription_unauthorized";
            
            nlohmann::json error = {
                {"error", "unauthorized"},
                {"message", "Not authorized to delete this subscription"}
            };
            
            std::string error_str = error.dump();
            response->payload.assign(error_str.begin(), error_str.end());
            
            return response;
        }
        
        // Delete the subscription
        subscriptions_.erase(it);
        
        // Create success response
        response->message_type = "subscription_deleted";
        
        nlohmann::json result = {
            {"id", subscription_id},
            {"status", "deleted"}
        };
        
        std::string result_str = result.dump();
        response->payload.assign(result_str.begin(), result_str.end());
    }
    catch (const std::exception& e) {
        logger_->error("Error deleting subscription: {}", e.what());
        
        response->message_type = "subscription_error";
        
        nlohmann::json error = {
            {"error", "bad_request"},
            {"message", e.what()}
        };
        
        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
    }
    
    return response;
}

int SubscriptionManager::notify_event(const std::string& event_type, 
                                     const nlohmann::json& event_data) {
    
    logger_->info("Notifying subscribers of event type: {}", event_type);
    
    int notification_count = 0;
    std::vector<Subscription> matching_subscriptions;
    
    // Find matching subscriptions
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        for (const auto& [id, subscription] : subscriptions_) {
            if (subscription.active && subscription.event_type == event_type) {
                // Check if subscription has expired
                if (std::chrono::system_clock::now() > subscription.expiry_time) {
                    continue;  // Skip expired subscriptions
                }
                
                // Check filter criteria
                bool matches_filters = true;
                for (const auto& [key, value] : subscription.filter_criteria) {
                    if (!event_data.contains(key) || event_data[key].dump() != value) {
                        matches_filters = false;
                        break;
                    }
                }
                
                if (matches_filters) {
                    matching_subscriptions.push_back(subscription);
                }
            }
        }
    }
    
    // Send notifications
    for (const auto& subscription : matching_subscriptions) {
        if (send_notification(subscription, event_data)) {
            notification_count++;
        }
    }
    
    logger_->info("Sent {} notifications for event type: {}", 
                 notification_count, event_type);
    
    return notification_count;
}

int SubscriptionManager::cleanup_expired_subscriptions() {
    logger_->info("Cleaning up expired subscriptions");
    
    int removed_count = 0;
    auto now = std::chrono::system_clock::now();
    
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    auto it = subscriptions_.begin();
    while (it != subscriptions_.end()) {
        if (now > it->second.expiry_time) {
            logger_->debug("Removing expired subscription: {}", it->first);
            it = subscriptions_.erase(it);
            removed_count++;
        } else {
            ++it;
        }
    }
    
    logger_->info("Removed {} expired subscriptions", removed_count);
    return removed_count;
}

bool SubscriptionManager::validate_subscription(const Subscription& subscription) {
    // Basic validation
    if (subscription.event_type.empty()) {
        logger_->error("Subscription event type cannot be empty");
        return false;
    }
    
    if (subscription.notification_url.empty() && subscription.callback_service.empty()) {
        logger_->error("Subscription must have either notification_url or callback_service");
        return false;
    }
    
    // TODO: Add more validation as needed
    
    return true;
}

std::string SubscriptionManager::generate_subscription_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex_chars = "0123456789abcdef";
    
    std::stringstream ss;
    ss << "sub-";
    for (int i = 0; i < 16; ++i) {
        ss << hex_chars[dis(gen)];
    }
    
    return ss.str();
}

bool SubscriptionManager::send_notification(const Subscription& subscription, 
                                           const nlohmann::json& event_data) {
    logger_->debug("Sending notification for subscription {} (event type: {})", 
                  subscription.id, subscription.event_type);
    
    try {
        // For internal callbacks (between AF components)
        if (!subscription.callback_service.empty()) {
            auto comm_it = orchestrator_->get_communication_services().find(subscription.callback_service);
            if (comm_it == orchestrator_->get_communication_services().end()) {
                logger_->error("Communication service not found for subscription callback: {}", 
                              subscription.callback_service);
                return false;
            }
            
            auto& comm_service = comm_it->second;
            
            // Create notification message
            auto msg = std::make_shared<af::communication::Message>();
            msg->message_type = "event_notification";
            
            // Add relevant metadata
            msg->metadata["event_type"] = subscription.event_type;
            msg->metadata["subscription_id"] = subscription.id;
            
            // Prepare payload
            nlohmann::json notification = {
                {"event_type", subscription.event_type},
                {"subscription_id", subscription.id},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
                {"data", event_data}
            };
            
            std::string notification_str = notification.dump();
            msg->payload.assign(notification_str.begin(), notification_str.end());
            
            // Send notification asynchronously
            comm_service->send_async(subscription.callback_service, msg);
            
            return true;
        }
        
        // For external notifications (via HTTP webhook)
        else if (!subscription.notification_url.empty()) {
            // TODO: Implement HTTP client for external notifications
            // For now, we'll just log that we would send a notification
            logger_->info("Would send external notification to URL: {}", 
                         subscription.notification_url);
            
            // In a real implementation, this would make an HTTP POST request
            // to the notification_url with the event data
            
            return true;
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error sending notification: {}", e.what());
    }
    
    return false;
}

} // namespace core
} // namespace af