/**
 * @file qod_session_manager.h
 * @brief Manages CAMARA QualityOnDemand sessions
 *
 * This class is responsible for managing QoD sessions lifecycle, including
 * creation, retrieval, extension, and deletion. It interfaces with the PCF
 * for actual QoS policy enforcement.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <optional>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>
#include <spdlog/spdlog.h>
#include <unordered_set>
#include "../../common/models/qod/qod_session.h"
#include "../../common/models/qod/qod_events.h"
#include "ue_state_manager.h"
#include "../../common/communication/include/message.h"

namespace af {
namespace core {
    class AfOrchestrator; // Forward declaration
}

namespace qod {

/**
 * @brief Configuration for QoD session management
 */
struct QodSessionConfig {
    std::chrono::seconds max_session_duration{86400};      // 24 hours default max
    std::chrono::seconds min_session_duration{60};         // 1 minute min
    std::chrono::seconds session_cleanup_interval{60};     // Cleanup check interval
    std::chrono::seconds unavailable_session_ttl{360};     // TTL for unavailable sessions
    bool enable_notifications{true};                       // Enable CloudEvents notifications
    std::string api_base_url{"https://api.example.com/qod/v1"}; // Base URL for events
    std::unordered_map<std::string, std::chrono::seconds> qos_profile_max_durations;
};

/**
 * @brief Manages CAMARA QualityOnDemand sessions
 */
class QodSessionManager {
public:
    /**
     * @brief Constructor
     * @param ue_state_manager Shared pointer to UE state manager
     * @param qod_state_manager Shared pointer to QoD state manager
     * @param config Configuration for QoD session management
     */
    QodSessionManager(
        std::shared_ptr<UeStateManager> ue_state_manager,
        std::shared_ptr<QodStateManager> qod_state_manager,
        const QodSessionConfig& config = QodSessionConfig{});
    
    /**
     * @brief Destructor
     */
    ~QodSessionManager();
    
    /**
     * @brief Initialize the QoD session manager
     * @param orchestrator Pointer to the orchestrator
     */
    void initialize(af::core::AfOrchestrator* orchestrator);
    
    /**
     * @brief Start the session manager (cleanup threads, etc.)
     */
    void start();
    
    /**
     * @brief Stop the session manager
     */
    void stop();
    
    // === Session Management Operations ===
    
    /**
     * @brief Create a new QoD session
     * @param request Session creation request
     * @return Created session or nullopt on failure
     */
    std::optional<af::common::qod::QodSession> create_session(const af::common::qod::CreateSessionRequest& request);
    
    /**
     * @brief Get session information
     * @param session_id Session ID
     * @param api_consumer_id API consumer ID for authorization
     * @return Session information or nullopt if not found/unauthorized
     */
    std::optional<af::common::qod::QodSession> get_session(
        const std::string& session_id,
        const std::string& api_consumer_id);
    
    /**
     * @brief Delete a session
     * @param session_id Session ID
     * @param api_consumer_id API consumer ID for authorization
     * @return true if successful, false otherwise
     */
    bool delete_session(
        const std::string& session_id,
        const std::string& api_consumer_id);
    
    /**
     * @brief Extend session duration
     * @param request Extension request
     * @param api_consumer_id API consumer ID for authorization
     * @return Updated session or nullopt on failure
     */
    std::optional<af::common::qod::QodSession> extend_session_duration(
        const af::common::qod::ExtendSessionDurationRequest& request,
        const std::string& api_consumer_id);
    
    /**
     * @brief Retrieve sessions for a device
     * @param request Retrieval request
     * @return Vector of sessions for the device
     */
    std::vector<af::common::qod::QodSession> retrieve_sessions_by_device(
        const af::common::qod::RetrieveSessionsRequest& request);
    
    // === PCF Integration ===
    
    /**
     * @brief Handle PCF session creation response
     * @param session_id QoD session ID
     * @param pcf_session_id PCF session ID
     * @param success Whether PCF session creation succeeded
     * @param error_message Error message if failed
     */
    void handle_pcf_session_response(
        const std::string& session_id,
        const std::string& pcf_session_id,
        bool success,
        const std::string& error_message = "");
    
    /**
     * @brief Handle PCF session termination notification
     * @param pcf_session_id PCF session ID
     * @param reason Termination reason
     */
    void handle_pcf_session_terminated(
        const std::string& pcf_session_id,
        const std::string& reason);
    
    // === Notification Management ===
    
    /**
     * @brief Set notification delivery handler
     * @param handler Notification delivery implementation
     */
    void set_notification_handler(
        std::shared_ptr<af::common::qod::INotificationDelivery> handler);
    
    /**
     * @brief Get all active sessions
     * @return Vector of all active sessions
     */
    std::vector<af::common::qod::QodSession> get_all_sessions();
    
    /**
     * @brief Get sessions by status
     * @param status QoS status to filter by
     * @return Vector of sessions with the specified status
     */
    std::vector<af::common::qod::QodSession> get_sessions_by_status(af::common::qod::QosStatus status);

    /**
     * @brief Handle PDU Session Terminated Event
     * @param event Event containing SUPI and PDU session ID
     * @return void
     */
    void handle_pdu_session_terminated(const af::core::events::PduSessionTerminatedEvent& event);

    /**
     * @brief Initialize logger
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // === Internal Methods ===
    
    /**
     * @brief Resolve device to internal identifiers
     * @param device QoD device specification
     * @return Resolved SUPI and PDU session info if found
     */
    std::pair<std::optional<Supi>, std::optional<std::string>> 
        resolve_device(const af::common::qod::QodDevice& device);
    
    /**
     * @brief Validate QoS profile
     * @param profile QoS profile name
     * @return true if profile is valid and available
     */
    bool validate_qos_profile(const std::string& profile);
    
    /**
     * @brief Check for conflicting sessions
     * @param device Device identifier
     * @param app_server Application server
     * @return Session ID if conflict exists
     */
    std::optional<std::string> check_session_conflict(
        const af::common::qod::QodDevice& device,
        const af::common::qod::ApplicationServer& app_server);
    
    /**
     * @brief Apply session to PCF
     * @param session Session to apply
     * @return true if successfully sent to PCF
     */
    bool apply_session_to_pcf(af::common::qod::QodSession& session);
    
    /**
     * @brief Remove session from PCF
     * @param session Session to remove
     * @return true if successfully removed
     */
    bool remove_session_from_pcf(const af::common::qod::QodSession& session);
    
    /**
     * @brief Update session in PCF (for extension)
     * @param session Session to update
     * @param new_duration New total duration
     * @return true if successfully updated
     */
    bool update_session_in_pcf(af::common::qod::QodSession& session, std::chrono::seconds new_duration);
    
    /**
     * @brief Send notification for session status change
     * @param session Session that changed
     * @param old_status Previous status
     * @param status_info Additional status information
     */
    void send_status_change_notification(
        const af::common::qod::QodSession& session,
        af::common::qod::QosStatus old_status,
        std::optional<af::common::qod::StatusInfo> status_info = std::nullopt);
    
    /**
     * @brief Cleanup expired and terminated sessions
     */
    void cleanup_expired_sessions();
    
    /**
     * @brief Session cleanup thread function
     */
    void session_cleanup_thread();
    
    /**
     * @brief Generate a unique session ID
     * @return UUID format session ID
     */
    std::string generate_session_id();
    
    /**
     * @brief Build PCF request for QoD session
     * @param session QoD session
     * @return JSON payload for PCF request
     */
    nlohmann::json build_pcf_request(const af::common::qod::QodSession& session);
    
    /**
     * @brief Convert device identifiers to PCF format
     * @param device QoD device
     * @param supi Resolved SUPI
     * @return PCF-compatible identifier
     */
    nlohmann::json convert_device_to_pcf(
        const af::common::qod::QodDevice& device,
        const std::optional<Supi>& supi);
    
    /**
     * @brief Build flow filters for PCF
     * @param session QoD session
     * @return Flow description for PCF
     */
    nlohmann::json build_flow_filters(const af::common::qod::QodSession& session);
    
    /**
     * @brief Get maximum duration for QoS profile
     * @param profile QoS profile name
     * @return Maximum allowed duration
     */
    std::chrono::seconds get_max_duration_for_profile(const std::string& profile);
    
    // === Member Variables ===
    
    // Core dependencies
    af::core::AfOrchestrator* orchestrator_;
    std::shared_ptr<UeStateManager> ue_state_manager_;
    std::shared_ptr<QodStateManager> qod_state_manager_;
    
    // Configuration
    QodSessionConfig config_;
    
    // Notification handling
    std::shared_ptr<af::common::qod::INotificationDelivery> notification_handler_;
    
    // Session cleanup
    std::thread cleanup_thread_;
    std::atomic<bool> cleanup_running_{false};
    
    // Logger
    std::shared_ptr<spdlog::logger> logger_;
    
    // Supported QoS profiles (would be loaded from configuration)
    std::unordered_set<std::string> supported_profiles_{
        "QOS_E", "QOS_S", "QOS_M", "QOS_L",  // Standard CAMARA profiles
        "voice", "video", "game", "data"      // Custom profiles
    };
};

} // namespace qod
} // namespace af