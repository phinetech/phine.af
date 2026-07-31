#pragma once
/// @file qod_client.hpp
/// @brief Low-level gRPC client for CAMARA QoD operations on phine.af (af_core).
///
/// QodClient sends InternalMessage RPCs to af_core's gRPC server, with JSON
/// payloads that follow the CAMARA Quality-on-Demand API conventions.
///
/// Thread-safety: QodClient is safe to call from multiple threads.  The
/// underlying gRPC channel and stub are thread-safe, and no mutable state is
/// accessed without synchronisation.

#include "qod_models.hpp"

#include <grpcpp/grpcpp.h>
#include <message.grpc.pb.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace phine::adapter {

// ─── Configuration ──────────────────────────────────────────────────────────

struct QodClientConfig {
    std::string af_core_address = "localhost:50051";
    std::string transport = "grpc";  // "grpc" or "http"
    int timeout_seconds = 30;
    int max_retries = 3;
    int initial_retry_delay_ms = 1000;
};

// ─── Interface (for testability) ────────────────────────────────────────────

/// Abstract interface consumed by SessionManager.
class IQodClient {
   public:
    virtual ~IQodClient() = default;

    virtual bool wait_for_ready(int timeout_seconds = 30) = 0;

    virtual Result<SessionInfo> create_session(
        const CreateSessionRequest& request) = 0;

    virtual Result<SessionInfo> get_session(
        const std::string& session_id) = 0;

    virtual Result<void> delete_session(const std::string& session_id) = 0;

    virtual Result<SessionInfo> extend_session(
        const std::string& session_id,
        const ExtendSessionRequest& request) = 0;

    virtual Result<std::vector<SessionInfo>> retrieve_sessions(
        const Device& device) = 0;
};

// ─── Concrete Implementation ────────────────────────────────────────────────

class QodClient final : public IQodClient {
   public:
    explicit QodClient(const QodClientConfig& config);
    ~QodClient() override = default;

    // Non-copyable, non-movable (holds gRPC resources)
    QodClient(const QodClient&) = delete;
    QodClient& operator=(const QodClient&) = delete;

    bool wait_for_ready(int timeout_seconds = 30) override;

    Result<SessionInfo> create_session(
        const CreateSessionRequest& request) override;

    Result<SessionInfo> get_session(
        const std::string& session_id) override;

    Result<void> delete_session(const std::string& session_id) override;

    Result<SessionInfo> extend_session(
        const std::string& session_id,
        const ExtendSessionRequest& request) override;

    Result<std::vector<SessionInfo>> retrieve_sessions(
        const Device& device) override;

   private:
    /// Send an InternalMessage with retry logic.
    /// Returns the raw JSON response payload on success.
    Result<std::string> send_message(
        const std::string& message_type,
        const std::string& json_payload,
        const std::map<std::string, std::string>& metadata);

    /// Determine whether a gRPC status code warrants a retry.
    static bool is_retryable(grpc::StatusCode code);

    /// Generate a UUID-v4 correlation ID.
    static std::string generate_correlation_id();

    // ── JSON helpers ────────────────────────────────────────────────────────
    static std::string serialise_create_request(
        const CreateSessionRequest& req);
    static std::string serialise_extend_request(
        const ExtendSessionRequest& req);
    static std::string serialise_device(const Device& dev);
    static SessionInfo parse_session_info(const std::string& json);
    static std::vector<SessionInfo> parse_session_list(
        const std::string& json);

    // ── Data members (immutable after construction) ─────────────────────────
    const QodClientConfig config_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<af::proto::InternalCommunication::Stub> stub_;
};

}  // namespace phine::adapter
