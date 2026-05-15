/// @file session_manager.cpp
/// @brief SessionManager implementation — lifecycle orchestration for QoD sessions.
///
/// Thread-safety: All methods are called sequentially from main's control loop.
/// No internal locks are required.

#include "session_manager.hpp"

#include <spdlog/spdlog.h>

namespace phine::adapter {

// ─── Construction ───────────────────────────────────────────────────────────

SessionManager::SessionManager(std::shared_ptr<IQodClient> client)
    : client_(std::move(client)) {
    spdlog::info("[SessionManager] Initialised");
}

// ─── Single-stream creation ─────────────────────────────────────────────────

Result<SessionInfo> SessionManager::request_qos_for_stream(
    const StreamConfig& stream) {
    spdlog::info("[SessionManager] Requesting QoS for '{}' (profile={}, "
                 "duration={}s)",
                 stream.name, stream.qos_profile, stream.duration_seconds);

    CreateSessionRequest req;
    req.device = stream.device;
    req.application_server = stream.app_server;
    req.device_ports = stream.device_ports;
    req.qos_profile = stream.qos_profile;
    req.duration_seconds = stream.duration_seconds;

    auto result = client_->create_session(req);
    if (!result.success) {
        spdlog::error(
            "[SessionManager] Failed to create session for '{}': {} — {}",
            stream.name, result.error_code, result.error_message);
        return result;
    }

    // Track the session
    TrackedSession tracked;
    tracked.stream = stream;
    tracked.session_info = result.value;
    tracked.created_at = std::chrono::system_clock::now();
    tracked.last_checked = tracked.created_at;

    const auto& sid = result.value.session_id;
    sessions_[sid] = std::move(tracked);

    spdlog::info(
        "[SessionManager] Session created for '{}': id={}, status={}",
        stream.name, sid, to_string(result.value.qos_status));

    return result;
}

// ─── Query ──────────────────────────────────────────────────────────────────

Result<SessionInfo> SessionManager::get_session_status(
    const std::string& session_id) {
    auto result = client_->get_session(session_id);
    if (result.success) {
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second.session_info = result.value;
            it->second.last_checked = std::chrono::system_clock::now();
            it->second.check_count++;
        }
    }
    return result;
}

// ─── Deletion ───────────────────────────────────────────────────────────────

Result<void> SessionManager::delete_session(const std::string& session_id) {
    auto result = client_->delete_session(session_id);
    if (result.success) {
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second.cleanup_done = true;
        }
        sessions_.erase(session_id);
        spdlog::info("[SessionManager] Session {} deleted", session_id);
    } else {
        spdlog::error("[SessionManager] Failed to delete session {}: {} — {}",
                      session_id, result.error_code, result.error_message);
    }
    return result;
}

// ─── Batch creation ─────────────────────────────────────────────────────────

void SessionManager::create_all_sessions(
    const std::vector<StreamConfig>& streams) {
    spdlog::info("[SessionManager] Creating {} session(s)…", streams.size());

    int success_count = 0;
    for (const auto& stream : streams) {
        auto result = request_qos_for_stream(stream);
        if (result.success) {
            ++success_count;
        }
    }

    spdlog::info("[SessionManager] Created {}/{} session(s) successfully",
                 success_count, streams.size());
}

// ─── Monitoring ─────────────────────────────────────────────────────────────

void SessionManager::monitor_sessions() {
    if (sessions_.empty()) {
        spdlog::info("[SessionManager] No sessions to monitor");
        return;
    }

    spdlog::info("[SessionManager] Monitoring {} session(s)…",
                 sessions_.size());

    for (auto& [sid, tracked] : sessions_) {
        auto result = client_->get_session(sid);
        if (!result.success) {
            spdlog::warn(
                "[SessionManager] Failed to query session {} ('{}'): {}",
                sid, tracked.stream.name, result.error_code);
            continue;
        }

        // Detect status change
        QosStatus old_status = tracked.session_info
                                   ? tracked.session_info->qos_status
                                   : QosStatus::REQUESTED;
        QosStatus new_status = result.value.qos_status;

        if (new_status != old_status) {
            spdlog::info(
                "[SessionManager] Session {} ('{}') status: {} → {}",
                sid, tracked.stream.name, to_string(old_status),
                to_string(new_status));
        }

        tracked.session_info = result.value;
        tracked.last_checked = std::chrono::system_clock::now();
        tracked.check_count++;
    }
}

// ─── Cleanup ────────────────────────────────────────────────────────────────

void SessionManager::cleanup_all_sessions() {
    spdlog::info("[SessionManager] Cleaning up {} session(s)…",
                 sessions_.size());

    // Collect IDs first to avoid iterator invalidation during erasure.
    std::vector<std::string> ids;
    ids.reserve(sessions_.size());
    for (const auto& [sid, _] : sessions_) {
        ids.push_back(sid);
    }

    int deleted = 0;
    for (const auto& sid : ids) {
        auto it = sessions_.find(sid);
        if (it == sessions_.end()) continue;

        spdlog::info("[SessionManager] Deleting session {} ('{}')",
                     sid, it->second.stream.name);

        auto result = client_->delete_session(sid);
        if (result.success) {
            sessions_.erase(sid);
            ++deleted;
        } else {
            spdlog::error(
                "[SessionManager] Failed to delete session {}: {} — {}",
                sid, result.error_code, result.error_message);
            it->second.cleanup_done = false;
        }
    }

    spdlog::info("[SessionManager] Cleanup complete: {}/{} deleted",
                 deleted, ids.size());
}

// ─── Accessors ──────────────────────────────────────────────────────────────

const std::map<std::string, TrackedSession>&
SessionManager::tracked_sessions() const {
    return sessions_;
}

}  // namespace phine::adapter
