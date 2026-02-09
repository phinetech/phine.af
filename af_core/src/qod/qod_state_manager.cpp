/**
 * @file qod_state_manager.cpp
 * @brief Implementation of the QoD state manager
 */

#include "qod/qod_state_manager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace af {
namespace qod {

QodStateManager::QodStateManager() {
    // Setup logger
    // Setup logger
    initializeLogger(spdlog::level::debug);

    // Initialize default QoS profile mappings
    initialize_default_mappings();

    logger_->info("QoD State Manager created");
}

QodStateManager::~QodStateManager() {
    // Clean up resources
}

void QodStateManager::initializeLogger(spdlog::level::level_enum log_level) {
    logger_ = spdlog::get("qod_state_mgr");

    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("qod_session_mgr");
    }

    logger_->set_level(log_level);
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void QodStateManager::initialize_default_mappings() {
    // CAMARA standard profiles
    // Note: Priority values must be 1-16 per 3GPP TS 29.514 ReservPriority (PRIO_1 to PRIO_16)
    // Bandwidth values use TS 29.571 BitRate format: "<value> <unit>"
    qos_profile_mappings_["QOS_E"] = {
        1,          // 5QI = 1 (Conversational Voice)
        2,          // Priority (PRIO_2 - highest priority conversational)
        100,        // Packet Delay Budget (ms)
        0.001,      // Packet Error Rate (10^-3)
        std::nullopt, // Max Data Burst
        true,       // GBR
        "64 Kbps",  // Guaranteed UL
        "64 Kbps",  // Guaranteed DL
        "128 Kbps", // Max UL
        "128 Kbps"  // Max DL
    };

    qos_profile_mappings_["QOS_S"] = {
        2,          // 5QI = 2 (Conversational Video)
        4,          // Priority (PRIO_4 - high priority video)
        150,        // Packet Delay Budget (ms)
        0.001,      // Packet Error Rate
        std::nullopt,
        true,       // GBR
        "384 Kbps", // Guaranteed UL
        "384 Kbps", // Guaranteed DL
        "512 Kbps", // Max UL
        "512 Kbps"  // Max DL
    };

    qos_profile_mappings_["QOS_M"] = {
        3,          // 5QI = 3 (Real Time Gaming)
        3,          // Priority (PRIO_3 - high priority gaming)
        50,         // Packet Delay Budget (ms)
        0.001,      // Packet Error Rate
        std::nullopt,
        true,       // GBR
        "512 Kbps", // Guaranteed UL
        "512 Kbps", // Guaranteed DL
        "1024 Kbps", // Max UL
        "1024 Kbps"  // Max DL
    };

    qos_profile_mappings_["QOS_L"] = {
        4,          // 5QI = 4 (Non-Conversational Video)
        5,          // Priority (PRIO_5 - medium priority buffered video)
        300,        // Packet Delay Budget (ms)
        0.000001,   // Packet Error Rate (10^-6)
        std::nullopt,
        true,       // GBR
        "256 Kbps", // Guaranteed UL
        "256 Kbps", // Guaranteed DL
        "512 Kbps", // Max UL
        "512 Kbps"  // Max DL
    };

    // Custom profiles
    qos_profile_mappings_["voice"] = {
        1, 2, 100, 0.001, std::nullopt, true, "64 Kbps", "64 Kbps", "128 Kbps", "128 Kbps"
    };

    qos_profile_mappings_["video"] = {
        2, 4, 150, 0.001, std::nullopt, true, "1024 Kbps", "2048 Kbps", "2048 Kbps", "4096 Kbps"
    };

    qos_profile_mappings_["game"] = {
        3, 3, 50, 0.001, std::nullopt, true, "512 Kbps", "512 Kbps", "1024 Kbps", "1024 Kbps"
    };

    qos_profile_mappings_["data"] = {
        9,          // 5QI = 9 (Default non-GBR)
        9,          // Priority (PRIO_9 - default best effort)
        300,        // Packet Delay Budget (ms)
        0.000001,   // Packet Error Rate
        std::nullopt,
        false,      // Non-GBR
        std::nullopt, std::nullopt, std::nullopt, std::nullopt
    };
}

void QodStateManager::add_session(const af::common::qod::QodSession& session) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    sessions_by_id_[session.session_id] = session;
    logger_->debug("Added QoD session: {}", session.session_id);

    // Add to secondary index if SUPI is present
    if (session.ue_supi) {
        sessions_by_supi_[session.ue_supi->value].insert(session.session_id);
        logger_->debug("Indexed session {} under SUPI {}", session.session_id, session.ue_supi->value);
    }
}

std::optional<af::common::qod::QodSession> QodStateManager::get_session_by_id(const std::string& session_id) const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = sessions_by_id_.find(session_id);
    if (it != sessions_by_id_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool QodStateManager::remove_session(const std::string& session_id) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    // TODO: Update the southbound handlers when session is removed
    auto it = sessions_by_id_.find(session_id);
    if (it == sessions_by_id_.end()) {
        return false; // Session not found
    }

    const auto& session_to_remove = it->second;

    // Remove from secondary index if SUPI is present
    if (session_to_remove.ue_supi) {
        auto supi_it = sessions_by_supi_.find(session_to_remove.ue_supi->value);
        if (supi_it != sessions_by_supi_.end()) {
            supi_it->second.erase(session_id);
            // If the set of sessions for this SUPI is now empty, remove the SUPI entry itself
            if (supi_it->second.empty()) {
                sessions_by_supi_.erase(supi_it);
            }
        }
    }

    // Remove from primary storage
    sessions_by_id_.erase(it);

    return true;
}

bool QodStateManager::update_session(const af::common::qod::QodSession& session) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto it = sessions_by_id_.find(session.session_id);
    if (it != sessions_by_id_.end()) {
        // Simple overwrite. Assumes SUPI does not change after creation.
        it->second = session;
        logger_->debug("Updated QoD session: {}", session.session_id);
        return true;
    }
    return false;
}

std::vector<std::string> QodStateManager::get_sessions_by_supi(const std::string& supi) const {
    std::shared_lock<std::shared_mutex> lock(mtx_);

    auto it = sessions_by_supi_.find(supi);
    if (it != sessions_by_supi_.end()) {
        // Convert the set to a vector for the return type
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }

    // Return an empty vector if no sessions are found for the SUPI
    return {};
}

std::vector<af::common::qod::QodSession> QodStateManager::get_sessions_by_status(af::common::qod::QosStatus status) const {
    std::shared_lock<std::shared_mutex> lock(mtx_);

    std::vector<af::common::qod::QodSession> result;
    result.reserve(sessions_by_id_.size());

    for (const auto& [id, session] : sessions_by_id_) {
        if (session.qos_status == status) {
            result.push_back(session);
        }
    }

    return result;
}

std::vector<af::common::qod::QodSession> QodStateManager::get_all_sessions() const {

    std::shared_lock<std::shared_mutex> lock(mtx_);

    std::vector<af::common::qod::QodSession> result;
    result.reserve(sessions_by_id_.size());

    for (const auto& [id, session] : sessions_by_id_) {
        result.push_back(session);
    }

    return result;
}

// Clear all stored sessions
void QodStateManager::clear_all_sessions() {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    sessions_by_id_.clear();
    sessions_by_supi_.clear();
    logger_->info("Cleared all QoD sessions from state manager");
}

// Update session status and optional status info
bool QodStateManager::update_session_status(const std::string& session_id,
    af::common::qod::QosStatus new_status,
    std::optional<af::common::qod::StatusInfo> status_info) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto it = sessions_by_id_.find(session_id);
    if (it != sessions_by_id_.end()) {
        it->second.qos_status = new_status;
        if (status_info) {
            it->second.status_info = status_info;
        }
        logger_->debug("Updated status of QoD session {}: new status={}, status info={}",
                       session_id, static_cast<int>(new_status),
                       status_info ? std::to_string(static_cast<int>(*status_info)) : "none");
        return true;
    }
    return false;
}

// Retrieve session by SUPI and PDU session ID
std::optional<af::common::qod::QodSession> QodStateManager::get_session_by_pdu_session(
    const Supi& supi,
    const std::string& pdu_session_id) const {

    std::shared_lock<std::shared_mutex> lock(mtx_);

    auto it = sessions_by_supi_.find(supi.value);
    if (it != sessions_by_supi_.end()) {
        for (const auto& session_id : it->second) {
            auto session_it = sessions_by_id_.find(session_id);
            if (session_it != sessions_by_id_.end()) {
                const auto& session = session_it->second;
                if (session.pdu_session_id && *session.pdu_session_id == pdu_session_id) {
                    return session;
                }
            }
        }
    }

    return std::nullopt; // No matching session found
}

// Retrieve expired sessions based on current time and grace period
std::vector<af::common::qod::QodSession> QodStateManager::get_expired_sessions(
    const std::chrono::system_clock::time_point& current_time,
    const std::chrono::seconds& grace_period) const {

    std::vector<af::common::qod::QodSession> expired_sessions;
    std::shared_lock<std::shared_mutex> lock(mtx_);
    for (const auto& [session_id, session] : sessions_by_id_) {
        if (session.expires_at) {
            auto expiration_with_grace = *session.expires_at + grace_period;
            if (current_time >= expiration_with_grace) {
                expired_sessions.push_back(session);
            }
        }
    }
    return expired_sessions;
}

void QodStateManager::update_pcf_session_map(
    std:: string pcf_session_id, std::string qod_session_id) {

    std::unique_lock<std::shared_mutex> lock(pcf_mapping_mutex_);
    pcf_session_mapping_[pcf_session_id] = qod_session_id;
}

std::optional<af::common::qod::QodSession> QodStateManager::get_session_by_pcf_session_id(
    const std::string& pcf_session_id) const {

    std::string qod_session_id;

    // Scope 1: Acquire pcf_mapping_mutex to lookup QoD session ID
    {
        std::shared_lock<std::shared_mutex> lock(pcf_mapping_mutex_);

        auto pcf_it = pcf_session_mapping_.find(pcf_session_id);
        if (pcf_it == pcf_session_mapping_.end()) {
            logger_->warn("No QoD session found for PCF session: {}", pcf_session_id);
            return std::nullopt;
        }

        qod_session_id = pcf_it->second;
    }

    // Scope 2: Acquire mtx_ to get session details
    return get_session_by_id(qod_session_id);
}

bool QodStateManager::remove_pcf_to_qod_session_mapping(
    const std::string& pcf_session_id) {

    std::unique_lock<std::shared_mutex> lock(pcf_mapping_mutex_);
    // Find and erase the mapping
    if (pcf_session_mapping_.find(pcf_session_id) == pcf_session_mapping_.end()) {
        logger_->warn("No mapping found for PCF session ID: {}", pcf_session_id);
        return false; // Not found
    }
    pcf_session_mapping_.erase(pcf_session_id);
    return true;
}

// === QoS Profile Mapping ===
// TODO: move to qos_profile_manager when available
std::optional<af::common::qod::QosProfileMapping> QodStateManager::get_qos_profile_mapping(
    const std::string& qos_profile) {

    std::shared_lock<std::shared_mutex> lock(mappings_mutex_);

    auto it = qos_profile_mappings_.find(qos_profile);
    if (it != qos_profile_mappings_.end()) {
        return it->second;
    }

    return std::nullopt;
}

void QodStateManager::register_qos_profile(const std::string& profile_name,
                                        const af::common::qod::QosProfileMapping& mapping) {
    std::unique_lock<std::shared_mutex> lock(mappings_mutex_);
    qos_profile_mappings_[profile_name] = mapping;
    logger_->info("Registered QoS profile mapping: {}", profile_name);
}

} // namespace qod
} // namespace af