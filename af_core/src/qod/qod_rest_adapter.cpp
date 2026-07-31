/**
 * @file qod_rest_adapter.cpp
 * @brief Implementation of the CAMARA QoD REST API adapter
 */

#include "qod/qod_rest_adapter.h"
#include <spdlog/spdlog.h>

namespace af {
namespace core {
namespace qod {

QodRestAdapter::QodRestAdapter(
    std::shared_ptr<rest::RestRouter> rest_router,
    std::shared_ptr<RequestRouter> request_router)
    : rest_router_(rest_router)
    , request_router_(request_router) {
}

void QodRestAdapter::register_endpoints(
    af::communication::CommunicationServicePtr http_service) {

    if (!http_service) {
        spdlog::error("QodRestAdapter: http_service is null");
        return;
    }

    spdlog::info("QodRestAdapter: Registering CAMARA QoD REST endpoints");

    // POST /quality-on-demand/v1/sessions → qod_create_session
    http_service->register_http_endpoint(
        "POST",
        "/quality-on-demand/v1/sessions",
        create_handler("qod_create_session", "/quality-on-demand/v1/sessions"));

    // GET /quality-on-demand/v1/sessions/{sessionId} → qod_get_session
    http_service->register_http_endpoint(
        "GET",
        "/quality-on-demand/v1/sessions/*",
        create_handler("qod_get_session", "/quality-on-demand/v1/sessions/{sessionId}"));

    // DELETE /quality-on-demand/v1/sessions/{sessionId} → qod_delete_session
    http_service->register_http_endpoint(
        "DELETE",
        "/quality-on-demand/v1/sessions/*",
        create_handler("qod_delete_session", "/quality-on-demand/v1/sessions/{sessionId}"));

    // POST /quality-on-demand/v1/sessions/{sessionId}/extend → qod_extend_session
    http_service->register_http_endpoint(
        "POST",
        "/quality-on-demand/v1/sessions/*/extend",
        create_handler("qod_extend_session", "/quality-on-demand/v1/sessions/{sessionId}/extend"));

    // POST /quality-on-demand/v1/retrieve-sessions → qod_retrieve_sessions
    http_service->register_http_endpoint(
        "POST",
        "/quality-on-demand/v1/retrieve-sessions",
        create_handler("qod_retrieve_sessions", "/quality-on-demand/v1/retrieve-sessions"));

    spdlog::info("QodRestAdapter: CAMARA QoD REST endpoints registered");
}

af::communication::HttpRequestHandler QodRestAdapter::create_handler(
    const std::string& message_type,
    const std::string& path_pattern) {

    return [this, message_type, path_pattern](
        const af::communication::HttpRequest& req) -> af::communication::HttpServerResponse {

        spdlog::debug("QodRestAdapter: Handling {} {} -> {}",
                     req.method, req.path, message_type);

        // Convert HTTP request to internal Message
        auto message = rest_router_->http_request_to_message(
            req, message_type, path_pattern);

        if (!message) {
            af::communication::HttpServerResponse error_resp;
            error_resp.status_code = 500;
            error_resp.body = R"({"error": "Failed to convert request"})";
            error_resp.headers["content-type"] = "application/json";
            return error_resp;
        }

        // Route to internal handler
        auto response_msg = request_router_->route_message(message);

        if (!response_msg) {
            af::communication::HttpServerResponse error_resp;
            error_resp.status_code = 500;
            error_resp.body = R"({"error": "Handler returned no response"})";
            error_resp.headers["content-type"] = "application/json";
            return error_resp;
        }

        // Convert Message response to HTTP response
        auto http_resp = rest_router_->message_to_http_response(response_msg);

        spdlog::debug("QodRestAdapter: Returning status {} for {}",
                     http_resp.status_code, message_type);

        return http_resp;
    };
}

} // namespace qod
} // namespace core
} // namespace af
