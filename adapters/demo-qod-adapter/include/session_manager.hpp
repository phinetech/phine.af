#pragma once
/// @file session_manager.hpp
/// @brief High-level QoD session lifecycle manager.
///
/// SessionManager creates, monitors, and cleans up QoD sessions for a set of
/// configured traffic streams.  It depends only on IQodClient.
///
/// Thread-safety: SessionManager is NOT designed for concurrent use from
/// multiple threads.  It is driven by a single control loop in main().
/// All mutable state (sessions_ map) is accessed sequentially — no locks
/// needed, no deadlock risk.

#include "qod_client.hpp"
#include "qod_models.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace phine::adapter {

// ─── Configuration ──────────────────────────────────────────────────────────

/// Describes a single traffic stream that needs a QoD session.
struct StreamConfig {
    std::string name;              // e.g. "video_stream"
    std::string description;       // human-readable
    Device device;
    ApplicationServer app_server;
    std::optional<PortsSpec> device_ports;
    std::string qos_profile;       // CAMARA profile, e.g. "QOS_L"
    int duration_seconds = 3600;
};

// ─── Tracked session state ──────────────────────────────────────────────────

struct TrackedSession {
    StreamConfig stream;
    std::optional<SessionInfo> session_info;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_checked;
    int check_count = 0;
    bool cleanup_done = false;
};

// ─── SessionManager ─────────────────────────────────────────────────────────

class SessionManager {
   public:
    /// @param client  Shared pointer to a QoD client (real or mock).
    explicit SessionManager(std::shared_ptr<IQodClient> client);
    ~SessionManager() = default;

    // Non-copyable
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    /// Create a QoD session for a single stream and start tracking it.
    Result<SessionInfo> request_qos_for_stream(const StreamConfig& stream);

    /// Query the current status of a tracked session by ID.
    Result<SessionInfo> get_session_status(const std::string& session_id);

    /// Delete a specific session and remove it from tracking.
    Result<void> delete_session(const std::string& session_id);

    /// Create sessions for all configured streams.
    void create_all_sessions(const std::vector<StreamConfig>& streams);

    /// Poll all tracked sessions and log any status changes.
    void monitor_sessions();

    /// Delete every tracked session (graceful shutdown).
    void cleanup_all_sessions();

    /// Read-only access to tracked sessions.
    const std::map<std::string, TrackedSession>& tracked_sessions() const;

   private:
    std::shared_ptr<IQodClient> client_;

    /// Sessions keyed by session_id.  Accessed only from the control thread.
    std::map<std::string, TrackedSession> sessions_;
};

}  // namespace phine::adapter
