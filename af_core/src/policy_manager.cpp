/**
 * @file policy_manager.cpp
 * @brief Implementation of the policy manager
 */

#include "policy_manager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <random>
#include <sstream>
#include "af_orchestrator.h"

namespace af {
namespace core {

PolicyManager::PolicyManager() {
    // Setup logger
    initializeLogger();
    
    logger_->info("Policy Manager created");
}

PolicyManager::~PolicyManager() {
    // Clean up resources
}

void PolicyManager::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("af_policy");
    
    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("af_policy");
    }
    
    // Set the log level
    logger_->set_level(log_level);
    
    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void PolicyManager::initialize(AfOrchestrator* orchestrator) {
    orchestrator_ = orchestrator;
    logger_->info("Policy Manager initialized");
}

af::communication::MessagePtr PolicyManager::handle_qos_request(
    const af::communication::MessagePtr& message) {
    
    logger_->info("Handling QoS request");
    
    try {
        // Extract payload as JSON
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto policy_data = nlohmann::json::parse(payload_str);
        
        // Check if this is a create, update, or delete operation
        std::string operation = "create";  // Default to create
        
        if (policy_data.contains("operation")) {
            operation = policy_data["operation"];
        }
        
        // Handle based on operation
        if (operation == "create") {
            auto policy = create_policy(policy_data);
            
            // Create success response
            auto response = std::make_shared<af::communication::Message>();
            response->message_type = "qos_success";
            response->correlation_id = message->correlation_id;
            
            // Convert policy to JSON and set as payload
            nlohmann::json result = {
                {"policy_id", policy.id},
                {"state", policy.state}
            };
            
            std::string result_str = result.dump();
            response->payload.assign(result_str.begin(), result_str.end());
            
            return response;
        }
        else if (operation == "update") {
            if (!policy_data.contains("policy_id")) {
                throw std::runtime_error("Missing policy_id for update operation");
            }
            
            std::string policy_id = policy_data["policy_id"];
            bool success = update_policy(policy_id, policy_data);
            
            // Create response
            auto response = std::make_shared<af::communication::Message>();
            response->correlation_id = message->correlation_id;
            
            if (success) {
                response->message_type = "qos_success";
                
                auto policy_opt = get_policy(policy_id);
                nlohmann::json result = {
                    {"policy_id", policy_id},
                    {"state", policy_opt ? policy_opt->state : "unknown"}
                };
                
                std::string result_str = result.dump();
                response->payload.assign(result_str.begin(), result_str.end());
            }
            else {
                response->message_type = "qos_error";
                
                nlohmann::json result = {
                    {"error", "update_failed"},
                    {"message", "Failed to update policy"}
                };
                
                std::string result_str = result.dump();
                response->payload.assign(result_str.begin(), result_str.end());
            }
            
            return response;
        }
        else if (operation == "delete") {
            if (!policy_data.contains("policy_id")) {
                throw std::runtime_error("Missing policy_id for delete operation");
            }
            
            std::string policy_id = policy_data["policy_id"];
            bool success = delete_policy(policy_id);
            
            // Create response
            auto response = std::make_shared<af::communication::Message>();
            response->correlation_id = message->correlation_id;
            
            if (success) {
                response->message_type = "qos_success";
                
                nlohmann::json result = {
                    {"policy_id", policy_id},
                    {"state", "deleted"}
                };
                
                std::string result_str = result.dump();
                response->payload.assign(result_str.begin(), result_str.end());
            }
            else {
                response->message_type = "qos_error";
                
                nlohmann::json result = {
                    {"error", "delete_failed"},
                    {"message", "Failed to delete policy"}
                };
                
                std::string result_str = result.dump();
                response->payload.assign(result_str.begin(), result_str.end());
            }
            
            return response;
        }
        else {
            throw std::runtime_error("Unknown operation: " + operation);
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error handling QoS request: {}", e.what());
        
        // Create error response
        auto response = std::make_shared<af::communication::Message>();
        response->message_type = "qos_error";
        response->correlation_id = message->correlation_id;
        
        nlohmann::json error = {
            {"error", "bad_request"},
            {"message", e.what()}
        };
        
        std::string error_str = error.dump();
        response->payload.assign(error_str.begin(), error_str.end());
        
        return response;
    }
}

QoSPolicy PolicyManager::create_policy(const nlohmann::json& policy_data) {
    logger_->info("Creating new QoS policy");
    
    QoSPolicy policy;
    
    // Generate a unique ID for this policy
    policy.id = generate_policy_id();
    
    // Extract policy parameters from JSON
    try {
        if (policy_data.contains("app_id")) {
            policy.app_id = policy_data["app_id"];
        }
        
        if (policy_data.contains("ue_ipv4")) {
            policy.ue_ipv4 = policy_data["ue_ipv4"];
        }
        
        if (policy_data.contains("ue_ipv6")) {
            policy.ue_ipv6 = policy_data["ue_ipv6"];
        }
        
        if (policy_data.contains("flow_direction")) {
            policy.flow_direction = policy_data["flow_direction"];
        } else {
            policy.flow_direction = "bidirectional";  // Default
        }
        
        if (policy_data.contains("qos_reference")) {
            policy.qos_reference = policy_data["qos_reference"];
        } else {
            policy.qos_reference = 9;  // Default 5QI
        }
        
        if (policy_data.contains("guaranteed_bandwidth")) {
            policy.guaranteed_bandwidth = policy_data["guaranteed_bandwidth"];
        } else {
            policy.guaranteed_bandwidth = 0;  // Default
        }
        
        if (policy_data.contains("maximum_bandwidth")) {
            policy.maximum_bandwidth = policy_data["maximum_bandwidth"];
        } else {
            policy.maximum_bandwidth = 0;  // Default
        }
        
        // Extract any additional parameters
        if (policy_data.contains("additional_params") && policy_data["additional_params"].is_object()) {
            for (auto& [key, value] : policy_data["additional_params"].items()) {
                policy.additional_params[key] = value.is_string() ? 
                    value.get<std::string>() : value.dump();
            }
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error parsing policy data: {}", e.what());
        throw std::runtime_error("Invalid policy data: " + std::string(e.what()));
    }
    
    // Validate the policy
    if (!validate_policy(policy)) {
        throw std::runtime_error("Invalid policy configuration");
    }
    
    // Set initial state
    policy.state = "inactive";
    
    // Apply the policy to the network
    if (apply_policy(policy)) {
        // Store the policy
        std::lock_guard<std::mutex> lock(policies_mutex_);
        policies_[policy.id] = policy;
        
        logger_->info("Created QoS policy with ID: {}", policy.id);
        return policy;
    }
    else {
        policy.state = "error";
        policy.error_reason = "Failed to apply policy to network";
        
        // Store the policy even though it failed (for reporting purposes)
        std::lock_guard<std::mutex> lock(policies_mutex_);
        policies_[policy.id] = policy;
        
        logger_->error("Failed to apply QoS policy with ID: {}", policy.id);
        throw std::runtime_error("Failed to apply policy to network");
    }
}

bool PolicyManager::update_policy(const std::string& policy_id, 
                                 const nlohmann::json& policy_data) {
    logger_->info("Updating QoS policy: {}", policy_id);
    
    // Find the existing policy
    std::lock_guard<std::mutex> lock(policies_mutex_);
    auto it = policies_.find(policy_id);
    
    if (it == policies_.end()) {
        logger_->error("Policy not found: {}", policy_id);
        return false;
    }
    
    QoSPolicy& policy = it->second;
    QoSPolicy old_policy = policy;  // Save for rollback
    
    // Update policy fields
    try {
        if (policy_data.contains("app_id")) {
            policy.app_id = policy_data["app_id"];
        }
        
        if (policy_data.contains("ue_ipv4")) {
            policy.ue_ipv4 = policy_data["ue_ipv4"];
        }
        
        if (policy_data.contains("ue_ipv6")) {
            policy.ue_ipv6 = policy_data["ue_ipv6"];
        }
        
        if (policy_data.contains("flow_direction")) {
            policy.flow_direction = policy_data["flow_direction"];
        }
        
        if (policy_data.contains("qos_reference")) {
            policy.qos_reference = policy_data["qos_reference"];
        }
        
        if (policy_data.contains("guaranteed_bandwidth")) {
            policy.guaranteed_bandwidth = policy_data["guaranteed_bandwidth"];
        }
        
        if (policy_data.contains("maximum_bandwidth")) {
            policy.maximum_bandwidth = policy_data["maximum_bandwidth"];
        }
        
        // Update additional parameters
        if (policy_data.contains("additional_params") && policy_data["additional_params"].is_object()) {
            for (auto& [key, value] : policy_data["additional_params"].items()) {
                policy.additional_params[key] = value.is_string() ? 
                    value.get<std::string>() : value.dump();
            }
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error updating policy data: {}", e.what());
        return false;
    }
    
    // Validate the updated policy
    if (!validate_policy(policy)) {
        logger_->error("Invalid policy configuration after update");
        policy = old_policy;  // Rollback to previous state
        return false;
    }
    
    // Apply the updated policy
    if (apply_policy(policy)) {
        logger_->info("Successfully updated QoS policy: {}", policy_id);
        return true;
    }
    else {
        logger_->error("Failed to apply updated QoS policy: {}", policy_id);
        policy = old_policy;  // Rollback to previous state
        return false;
    }
}

bool PolicyManager::delete_policy(const std::string& policy_id) {
    logger_->info("Deleting QoS policy: {}", policy_id);
    
    // Find the policy
    std::lock_guard<std::mutex> lock(policies_mutex_);
    auto it = policies_.find(policy_id);
    
    if (it == policies_.end()) {
        logger_->error("Policy not found: {}", policy_id);
        return false;
    }
    
    QoSPolicy& policy = it->second;
    
    // Remove the policy from the network
    if (remove_policy(policy)) {
        // Remove from our map
        policies_.erase(it);
        logger_->info("Successfully deleted QoS policy: {}", policy_id);
        return true;
    }
    else {
        logger_->error("Failed to remove QoS policy from network: {}", policy_id);
        return false;
    }
}

std::vector<QoSPolicy> PolicyManager::get_all_policies() {
    std::lock_guard<std::mutex> lock(policies_mutex_);
    
    std::vector<QoSPolicy> result;
    result.reserve(policies_.size());
    
    for (const auto& [id, policy] : policies_) {
        result.push_back(policy);
    }
    
    return result;
}

std::optional<QoSPolicy> PolicyManager::get_policy(const std::string& policy_id) {
    std::lock_guard<std::mutex> lock(policies_mutex_);
    
    auto it = policies_.find(policy_id);
    if (it != policies_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

bool PolicyManager::validate_policy(const QoSPolicy& policy) {
    // Basic validation
    if (policy.app_id.empty() && policy.ue_ipv4.empty() && policy.ue_ipv6.empty()) {
        logger_->error("Policy must specify at least one of: app_id, ue_ipv4, ue_ipv6");
        return false;
    }
    
    // Validate flow direction
    if (policy.flow_direction != "uplink" && 
        policy.flow_direction != "downlink" && 
        policy.flow_direction != "bidirectional") {
        logger_->error("Invalid flow direction: {}", policy.flow_direction);
        return false;
    }
    
    // Validate QoS reference (5QI value)
    if (policy.qos_reference < 1 || policy.qos_reference > 79) {
        logger_->error("Invalid QoS reference (5QI): {}", policy.qos_reference);
        return false;
    }
    
    // Validate bandwidth values
    if (policy.guaranteed_bandwidth < 0) {
        logger_->error("Guaranteed bandwidth cannot be negative: {}", policy.guaranteed_bandwidth);
        return false;
    }
    
    if (policy.maximum_bandwidth < 0) {
        logger_->error("Maximum bandwidth cannot be negative: {}", policy.maximum_bandwidth);
        return false;
    }
    
    if (policy.maximum_bandwidth > 0 && policy.guaranteed_bandwidth > policy.maximum_bandwidth) {
        logger_->error("Guaranteed bandwidth ({}) cannot exceed maximum bandwidth ({})", 
                      policy.guaranteed_bandwidth, policy.maximum_bandwidth);
        return false;
    }
    
    return true;
}

bool PolicyManager::apply_policy(QoSPolicy& policy) {
    logger_->info("Applying QoS policy: {}", policy.id);
    
    // TODO: In a real implementation, this would interact with the PCF
    // via a southbound interface handler to apply the policy.
    // For now, we'll simulate success.
    
    // Simulated PCF transaction ID
    policy.pcf_transaction_id = "pcf-tx-" + generate_policy_id();
    
    // Set policy state to active
    policy.state = "active";
    
    logger_->info("QoS policy applied successfully: {}", policy.id);
    return true;
}

bool PolicyManager::remove_policy(QoSPolicy& policy) {
    logger_->info("Removing QoS policy: {}", policy.id);
    
    // TODO: In a real implementation, this would interact with the PCF
    // via a southbound interface handler to remove the policy.
    // For now, we'll simulate success.
    
    logger_->info("QoS policy removed successfully: {}", policy.id);
    return true;
}

std::string PolicyManager::generate_policy_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex_chars = "0123456789abcdef";
    
    std::stringstream ss;
    ss << "policy-";
    for (int i = 0; i < 16; ++i) {
        ss << hex_chars[dis(gen)];
    }
    
    return ss.str();
}

} // namespace core
} // namespace af