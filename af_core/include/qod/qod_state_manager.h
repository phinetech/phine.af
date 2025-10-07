/**
 * @file qod_state_manager.h
 * @brief Manages state for CAMARA QualityOnDemand sessions
 * 
 * This class handles the storage, retrieval, and lifecycle management of
 * QualityOnDemand sessions.
 */

#pragma once

#include "../../common/models/qod/qod_session.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>

namespace af {
namespace qod {
/**
 * @brief Manages state for CAMARA QualityOnDemand sessions
 */
class QodStateManager {
public:
    /**
     * @brief Constructor
     */
    QodStateManager();

    /**
     * @brief Destructor
     */
    ~QodStateManager();

    /**
     * @brief Initialize default QoS profile mappings
     */
    void initialize_default_mappings();


    /**
     * @brief Add a new QoD session
     * @param session The QoD session to add
     */
    void add_session(const af::common::qod::QodSession& session);

    /**
     * @brief Retrieves a session by its unique ID.
     */
    std::optional<af::common::qod::QodSession> get_session_by_id(const std::string& session_id) const;

    /**
     * @brief Removes a session by its unique ID.
     * @return True if the session was found and removed.
     */
    bool remove_session(const std::string& session_id);

    /**
     * @brief Updates the state of an existing session.
     * @return True if the session was found and updated.
     */
    bool update_session(const af::common::qod::QodSession& session);

    /**
     * @brief Finds all session IDs associated with a specific SUPI.
     * Useful for cleanup or retrieval operations.
     */
    std::vector<std::string> get_sessions_by_supi(const std::string& supi) const;

    /**
     * @brief Find all sessions that match the status
     * @return A vector of sessions
     */
    std::vector<af::common::qod::QodSession> get_sessions_by_status(af::common::qod::QosStatus status) const;

    /**
     * @brief Get all stored sessions.
     * @return A vector of sessions
     */
    std::vector<af::common::qod::QodSession> get_all_sessions() const;

    /**
     * @brief Clears all stored sessions.
     * This is typically used during shutdown to free resources.
     */
    void clear_all_sessions();

    /**
     * @brief Updates the status and optional status info of a session.
     * @param session_id The unique ID of the session to update.
     * @param new_status The new QoS status to set.
     * @param status_info Optional additional status information.
     * @return True if the session was found and updated.
     */
    bool update_session_status(const std::string& session_id, 
        af::common::qod::QosStatus new_status, 
        std::optional<af::common::qod::StatusInfo> status_info = std::nullopt);

    /**
     * @brief Retrieves a session associated with a specific SUPI and PDU session ID.
     * @param supi The SUPI of the UE.
     * @param pdu_session_id The PDU session ID to match.
     * @return An optional QoD session if found, std::nullopt otherwise.
     */
    std::optional<af::common::qod::QodSession> get_session_by_pdu_session(
        const Supi& supi,
        const std::string& pdu_session_id) const;

    // get_expired_sessions
    /**
     * @brief Retrieves all sessions that have expired based on the current time and a grace period.
     * @param current_time The current system time.
     * @param grace_period The duration after expiration during which the session is still considered valid.
     * @return A vector of expired QoD sessions.
     */
    std::vector<af::common::qod::QodSession> get_expired_sessions(
        const std::chrono::system_clock::time_point& current_time,
        const std::chrono::seconds& grace_period) const;

    /**
     * @brief Update PCF session ID to QoD Session ID
     */
    void update_pcf_session_map(std::string pcf_session_id, std::string qod_session_id);

    /**
     * @brief Retrieves a session associated with a PCF session ID.
     * @param pcf_session_id The PCF session ID to match.
     * @return An optional QoD session if found, std::nullopt otherwise.
     */
    std::optional<af::common::qod::QodSession> get_session_by_pcf_session_id(
        const std::string& pcf_session_id) const;

    /**
     * @brief Remove PCF to QoD mapping
     * @return True if the session was found and removed.
     */
    bool remove_pcf_to_qod_session_mapping(
        const std::string& pcf_session_id);

    // === QoS Profile Mapping ===
    
    /**
     * @brief Get QoS profile mapping
     * @param qos_profile QoS profile name
     * @return Mapping configuration or nullopt if not found
     */
    std::optional<af::common::qod::QosProfileMapping> get_qos_profile_mapping(
        const std::string& qos_profile);
    
    /**
     * @brief Register custom QoS profile mapping
     * @param profile_name Profile name
     * @param mapping Mapping configuration
     */
    void register_qos_profile(const std::string& profile_name, 
                              const af::common::qod::QosProfileMapping& mapping);
private:

    /**
     * @brief Initialize logger
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);
    
    mutable std::mutex mtx_;

    // Primary storage: Keyed by the unique QoD session ID.
    std::unordered_map<std::string, af::common::qod::QodSession> sessions_by_id_;

    // Secondary index: Maps a UE's SUPI to all QoD sessions they own.
    // This is critical for efficient cleanup when a UE state changes.
    std::unordered_map<std::string /* supi */, std::unordered_set<std::string> /* session_ids */> sessions_by_supi_;

    // Secondary index: Maps a QoD session ID to PCF application session ID.
    std::unordered_map<std::string /* pcf_session_id */, std::string /* qod_session_id */> pcf_session_mapping_;
    // Note: The reverse mapping (PCF to QoD) is maintained in Qod Session Manager
    // because it needs to be accessed without locking this entire state manager.
    mutable std::mutex pcf_mapping_mutex_;

    // QoS profile mappings
    std::unordered_map<std::string, af::common::qod::QosProfileMapping> qos_profile_mappings_;
    mutable std::mutex mappings_mutex_;

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

};

} // namespace qod
} // namespace af