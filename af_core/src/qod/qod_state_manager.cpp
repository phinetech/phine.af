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
    logger_ = spdlog::get("qod_state_manager");
    
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("qod_state_manager");
    }
    
    logger_->info("QoD State Manager created");
}

QodStateManager::~QodStateManager() {
    // Clean up resources
}

void QodStateManager::add_session(const af::common::qod::QodSession& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_by_id_[session.session_id] = session;
    logger_->debug("Added QoD session: {}", session.session_id);

    // Add to secondary index if SUPI is present
    if (session.ue_supi) {
        sessions_by_supi_[*session.ue_supi].insert(session.session_id);
        logger_->debug("Indexed session {} under SUPI {}", session.session_id, *session.ue_supi);
    }
}

std::optional<af::common::qod::QodSession> QodStateManager::get_session_by_id(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_by_id_.find(session_id);
    if (it != sessions_by_id_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool QodStateManager::remove_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_by_id_.find(session_id);
    if (it == sessions_by_id_.end()) {
        return false; // Session not found
    }

    const auto& session_to_remove = it->second;

    // Remove from secondary index if SUPI is present
    if (session_to_remove.ue_supi) {
        auto supi_it = sessions_by_supi_.find(*session_to_remove.ue_supi);
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
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = sessions_by_supi_.find(supi);
    if (it != sessions_by_supi_.end()) {
        // Convert the set to a vector for the return type
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }

    // Return an empty vector if no sessions are found for the SUPI
    return {};
}

} // namespace qod
} // namespace af