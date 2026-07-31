#pragma once
/// @file http_qod_client.hpp
/// @brief HTTP client implementing IQodClient using CAMARA REST API.
///
/// This adapter calls the CAMARA Quality-on-Demand REST API endpoints directly:
///   - POST /quality-on-demand/v1/sessions
///   - GET /quality-on-demand/v1/sessions/{sessionId}
///   - DELETE /quality-on-demand/v1/sessions/{sessionId}
///   - POST /quality-on-demand/v1/sessions/{sessionId}/extend
///   - POST /quality-on-demand/v1/retrieve-sessions
///
/// Thread-safety: HttpQodClient is safe to call from multiple threads.
/// The underlying HTTP transport handles connection state internally.

#include "qod_models.hpp"
#include "qod_client.hpp"  // IQodClient interface + QodClientConfig

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace phine::adapter {

// ─── HTTP/2 QoD Client ──────────────────────────────────────────────────────

/// HTTP-based implementation of IQodClient.
/// Calls CAMARA QoD REST API endpoints directly.
class HttpQodClient final : public IQodClient {
   public:
    explicit HttpQodClient(const QodClientConfig& config);
    ~HttpQodClient() override = default;

    // Non-copyable
    HttpQodClient(const HttpQodClient&) = delete;
    HttpQodClient& operator=(const HttpQodClient&) = delete;

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
    /// Perform an HTTP request with retry logic.
    /// Returns {status_code, response_body} or {-1, error_message} on failure.
    struct HttpResult {
        int status_code{-1};
        std::string body;
        bool success{false};
    };
    HttpResult do_http_request(const std::string& method,
                               const std::string& path,
                               const std::string& body = "");

    /// Parse CAMARA error response
    Result<std::string> parse_camara_response(const HttpResult& result);

    /// Generate a UUID-v4 correlation ID.
    static std::string generate_correlation_id();

    // ── JSON helpers (reused from QodClient) ────────────────────────────────
    static std::string serialise_create_request(
        const CreateSessionRequest& req);
    static std::string serialise_extend_request(
        const ExtendSessionRequest& req);
    static std::string serialise_device(const Device& dev);
    static SessionInfo parse_session_info(const std::string& json);
    static std::vector<SessionInfo> parse_session_list(
        const std::string& json);

    // ── Data members ────────────────────────────────────────────────────────
    const QodClientConfig config_;
    std::string base_url_;   // e.g., "http://af_core:8080"
    std::string host_;
    std::string port_;
    bool use_tls_{false};
};

}  // namespace phine::adapter
