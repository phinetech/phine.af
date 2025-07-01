/**
 * @file policy_manager.h
 * @brief Manages QoS and other policies
 *
 * This class is responsible for managing QoS policies and other network policies
 * that the AF can influence. It interacts with the PCF via appropriate handlers.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include "../common/communication/include/message.h"
#include <nlohmann/json.hpp>
#include "ue_state_manager.h"

namespace af {
namespace core {

// Forward declaration
class AfOrchestrator;

/**
 * @brief Structure representing a QoS policy
 */
struct QoSPolicy {
    std::string id;
    std::string app_id;
    std::string ue_ipv4;
    std::string ue_ipv6;
    std::string flow_direction;  // uplink, downlink, bidirectional
    int qos_reference;           // 5QI value
    int guaranteed_bandwidth;    // in kbps
    int maximum_bandwidth;       // in kbps
    std::unordered_map<std::string, std::string> additional_params;
    std::string state;           // active, inactive, error
    std::string error_reason;
    std::string pcf_transaction_id;
};

/**
 * @brief Manages QoS and other network policies
 */
class PolicyManager {
public:
    /**
     * @brief Constructor
     */
    PolicyManager(std::shared_ptr<UeStateManager> ue_state_manager);
    
    /**
     * @brief Destructor
     */
    ~PolicyManager();
    
    /**
     * @brief Initialize the policy manager
     * @param orchestrator Pointer to the orchestrator
     */
    void initialize(AfOrchestrator* orchestrator);
    
    /**
     * @brief Handle a QoS request
     * @param message The QoS request message
     * @return Response message
     */
    af::communication::MessagePtr handle_qos_request(
        const af::communication::MessagePtr& message);
    
    /**
     * @brief Create a new QoS policy
     * @param policy_data JSON data describing the policy
     * @return The created policy
     */
    QoSPolicy create_policy(const nlohmann::json& policy_data);
    
    /**
     * @brief Update an existing QoS policy
     * @param policy_id ID of the policy to update
     * @param policy_data New policy data
     * @return true if successful, false otherwise
     */
    bool update_policy(const std::string& policy_id, 
                       const nlohmann::json& policy_data);
    
    /**
     * @brief Delete a QoS policy
     * @param policy_id ID of the policy to delete
     * @return true if successful, false otherwise
     */
    bool delete_policy(const std::string& policy_id);
    
    /**
     * @brief Get all policies
     * @return Vector of all policies
     */
    std::vector<QoSPolicy> get_all_policies();
    
    /**
     * @brief Get a specific policy
     * @param policy_id ID of the policy to get
     * @return The policy or nullopt if not found
     */
    std::optional<QoSPolicy> get_policy(const std::string& policy_id);

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Reference to orchestrator
    AfOrchestrator* orchestrator_;
    
    std::shared_ptr<UeStateManager> ue_state_manager_;

    // Map of policy ID to policy
    std::unordered_map<std::string, QoSPolicy> policies_;
    std::mutex policies_mutex_;
    
    // Logger
    std::shared_ptr<spdlog::logger> logger_;
    
    /**
     * @brief Validate a QoS policy
     * @param policy The policy to validate
     * @return true if valid, false otherwise
     */
    bool validate_policy(const QoSPolicy& policy);
    
    /**
     * @brief Apply a QoS policy to the network
     * @param policy The policy to apply
     * @return true if successful, false otherwise
     */
    bool apply_policy(QoSPolicy& policy);
    
    /**
     * @brief Remove a QoS policy from the network
     * @param policy The policy to remove
     * @return true if successful, false otherwise
     */
    bool remove_policy(QoSPolicy& policy);
    
    /**
     * @brief Generate a unique policy ID
     * @return A unique policy ID
     */
    std::string generate_policy_id();
};

} // namespace core
} // namespace af