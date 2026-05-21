/**
 * @file af_orchestrator.h
 * @brief Main orchestrator for the 5G Application Function core
 *
 * This class coordinates all functionality within the AF Core, managing
 * communication with northbound and southbound interfaces, policy decisions,
 * subscriptions, and overall state management.
 */

#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "request_router.h"
#include "policy_manager.h"
#include "subscription_manager.h"
#include "ue_state_manager.h"
#include "common/component/include/af_component.h"
#include "common/communication/include/communication_interface.h"
#include "af_typed_config.hpp"

// QoD includes
#include "qod/qod_session_manager.h"
#include "qod/qod_handler.h"
#include "qod/qod_notification_manager.h"
#include "events/event_dispatcher.h"

namespace af {
namespace core {

/**
 * @brief Main orchestrator for the AF core functionality
 */
class AfOrchestrator : public af::common::AfComponent {
public:
    /**
     * @brief Constructor
     * @param config_path Path to the configuration file
     */
    AfOrchestrator(const std::string& config_path);

    /**
     * @brief Destructor
     */
    ~AfOrchestrator();

    /**
     * @brief Initialize the orchestrator and its subcomponents
     */
    void initialize() override;

    /**
     * @brief Start the orchestrator and its services
     */
    void start() override;

    /**
     * @brief Stop the orchestrator and its services
     */
    void stop() override;

    /**
     * @brief Block until the orchestrator has been stopped.
     */
    void wait();

    /**
     * @brief Process a message from any source
     * @param message The incoming message
     * @return Response message
     */
    af::communication::MessagePtr process_message(
        const af::communication::MessagePtr& message);

    /**
     * @brief Get the RequestRouter instance
     * @return Shared pointer to RequestRouter
     */
    std::shared_ptr<RequestRouter> get_request_router() const { return request_router_; }

    /**
     * @brief Get the PolicyManager instance
     * @return Shared pointer to PolicyManager
     */
    std::shared_ptr<PolicyManager> get_policy_manager() const { return policy_manager_; }

    /**
     * @brief Get the SubscriptionManager instance
     * @return Shared pointer to SubscriptionManager
     */
    std::shared_ptr<SubscriptionManager> get_subscription_manager() const { return subscription_manager_; }

    /**
     * @brief Get the QodSessionManager instance
     * @return Shared pointer to QodSessionManager
     */
    std::shared_ptr<qod::QodSessionManager> get_qod_session_manager() const { return qod_session_manager_; }

    /**
     * @brief Get the communication services map
     * @return Map of communication service names to instances
     */
    const std::unordered_map<std::string, std::shared_ptr<af::communication::CommunicationService>>&
    get_communication_services() const {
        return communication_services_;
    }

    /**
     * @brief Get the configured destination used to reach the PCF handler.
     * @return Destination string for the PCF handler.
     */
    const std::string& get_pcf_destination() const { return pcf_destination_; }

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Configuration
    std::string config_path_;
    af::config::AppConfig app_config_{};
    std::string pcf_destination_{"pcf_handler:50055"};

    std::shared_ptr<af::core::events::EventDispatcher> event_dispatcher_;

    // Component instances
    std::shared_ptr<RequestRouter> request_router_;
    std::shared_ptr<PolicyManager> policy_manager_;
    std::shared_ptr<SubscriptionManager> subscription_manager_;
    std::shared_ptr<UeStateManager> ue_state_manager_;

    // QoD components
    std::shared_ptr<qod::QodSessionManager> qod_session_manager_;
    std::shared_ptr<qod::QodHandler> qod_handler_;
    std::shared_ptr<qod::QodNotificationManager> qod_notification_manager_;
    std::shared_ptr<qod::QodStateManager> qod_state_manager_;

    // Communication services for northbound and southbound interfaces
    std::unordered_map<std::string, std::shared_ptr<af::communication::CommunicationService>> communication_services_;

    // Message handler for incoming messages
    class OrchestratorMessageHandler;
    std::shared_ptr<OrchestratorMessageHandler> message_handler_;

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

    // Wait support
    mutable std::mutex wait_mutex_;
    std::condition_variable wait_cv_;

    /**
     * @brief Load configuration from file
     */
    void load_config();

    /**
     * @brief Initialize communication interfaces
     */
    void initialize_communication();

    /**
     * @brief Register message handlers
     */
    void register_handlers();

    /**
     * @brief Configure subcomponents
     */
    void configure_components();

    /**
     * @brief Initialize QoD components
     */
    void initialize_qod_components();

    /**
     * @brief Register QoD message handlers
     */
    void register_qod_handlers();
};

} // namespace core
} // namespace af