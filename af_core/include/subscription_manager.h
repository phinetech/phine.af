/**
 * @file subscription_manager.h
 * @brief Manages event subscriptions
 *
 * This class is responsible for managing event subscriptions from external
 * applications. It handles creation, retrieval, and deletion of subscriptions,
 * as well as notification delivery when events occur.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include <spdlog/spdlog.h>
#include "common/communication/include/message.h"
#include <nlohmann/json.hpp>
#include "ue_state_manager.h"

namespace af {
namespace core {

// Forward declaration
class AfOrchestrator;

/**
 * @brief Structure representing an event subscription
 */
struct Subscription {
    std::string id;
    std::string client_id;
    std::string event_type;
    std::string notification_url;
    std::unordered_map<std::string, std::string> filter_criteria;
    std::chrono::system_clock::time_point expiry_time;
    bool active;
    std::string callback_service;  // Service to notify for internal subscriptions
};

/**
 * @brief Manages event subscriptions
 */
class SubscriptionManager {
public:
    /**
     * @brief Constructor
     */
    SubscriptionManager(std::shared_ptr<UeStateManager> ue_state_manager);

    /**
     * @brief Destructor
     */
    ~SubscriptionManager();

    /**
     * @brief Initialize the subscription manager
     * @param orchestrator Pointer to the orchestrator
     */
    void initialize(AfOrchestrator* orchestrator);

    /**
     * @brief Create a new subscription
     * @param message Message containing subscription details
     * @return Response message with subscription ID
     */
    af::communication::MessagePtr create_subscription(
        const af::communication::MessagePtr& message);

    /**
     * @brief Get all subscriptions
     * @param message Message requesting subscriptions
     * @return Response message with subscription list
     */
    af::communication::MessagePtr get_all_subscriptions(
        const af::communication::MessagePtr& message);

    /**
     * @brief Get a specific subscription
     * @param message Message requesting a specific subscription
     * @return Response message with subscription details
     */
    af::communication::MessagePtr get_subscription(
        const af::communication::MessagePtr& message);

    /**
     * @brief Delete a subscription
     * @param message Message requesting deletion of a subscription
     * @return Response message indicating success/failure
     */
    af::communication::MessagePtr delete_subscription(
        const af::communication::MessagePtr& message);

    /**
     * @brief Notify subscribers of an event
     * @param event_type Type of event
     * @param event_data Event data
     * @return Number of notifications sent
     */
    int notify_event(const std::string& event_type,
                     const nlohmann::json& event_data);

    /**
     * @brief Clean up expired subscriptions
     * @return Number of subscriptions removed
     */
    int cleanup_expired_subscriptions();

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Reference to orchestrator
    AfOrchestrator* orchestrator_;

    // Reference to UE state manager for accessing UE subscription states
    std::shared_ptr<UeStateManager> ue_state_manager_;

    // Map of subscription ID to subscription
    std::unordered_map<std::string, Subscription> subscriptions_;
    std::mutex subscriptions_mutex_;

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

    /**
     * @brief Validate a subscription
     * @param subscription The subscription to validate
     * @return true if valid, false otherwise
     */
    bool validate_subscription(const Subscription& subscription);

    /**
     * @brief Generate a unique subscription ID
     * @return A unique subscription ID
     */
    std::string generate_subscription_id();

    /**
     * @brief Send a notification for a specific subscription
     * @param subscription The subscription to notify
     * @param event_data The event data
     * @return true if notification was sent successfully
     */
    bool send_notification(const Subscription& subscription,
                          const nlohmann::json& event_data);
};

} // namespace core
} // namespace af