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
#include <string>
#include <vector>
#include <mutex>
#include <optional>

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


private:
    mutable std::mutex mtx_;

    // Primary storage: Keyed by the unique QoD session ID.
    std::unordered_map<std::string, af::common::qod::QodSession> sessions_by_id_;

    // Secondary index: Maps a UE's SUPI to all QoD sessions they own.
    // This is critical for efficient cleanup when a UE state changes.
    std::unordered_map<std::string /* supi */, std::unordered_set<std::string> /* session_ids */> sessions_by_supi_;

    // Secondary index: Maps a QoD session ID to PCF application session ID.
    std::unordered_map<std::string /* qod_session_id */, std::string /* pcf_session_id */> pcf_session_mapping_;
    // Note: The reverse mapping (PCF to QoD) is maintained in Qod Session Manager
    // because it needs to be accessed without locking this entire state manager.
    mutable std::mutex pcf_mapping_mutex_;

};

} // namespace qod
} // namespace af