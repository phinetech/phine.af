/**
 * @file qod_rest_adapter.h
 * @brief CAMARA QoD REST API adapter
 *
 * This adapter maps CAMARA QoD REST API endpoints to internal message handlers.
 * It translates between the external REST API surface (CAMARA spec) and the
 * internal message-based architecture.
 *
 * Endpoint mappings:
 *   POST   /quality-on-demand/v1/sessions              → qod_create_session
 *   GET    /quality-on-demand/v1/sessions/{sessionId}  → qod_get_session
 *   DELETE /quality-on-demand/v1/sessions/{sessionId}  → qod_delete_session
 *   POST   /quality-on-demand/v1/sessions/{sessionId}/extend → qod_extend_session
 *   POST   /quality-on-demand/v1/retrieve-sessions     → qod_retrieve_sessions
 */

#pragma once

#include "rest_router.h"
#include "request_router.h"
#include "communication_interface.h"
#include <memory>

namespace af {
namespace core {
namespace qod {

/**
 * @brief Adapter for CAMARA QoD REST API endpoints
 */
class QodRestAdapter {
public:
    /**
     * @brief Construct the QoD REST adapter
     *
     * @param rest_router REST router for HTTP/Message conversion
     * @param request_router Request router for dispatching to handlers
     */
    QodRestAdapter(
        std::shared_ptr<rest::RestRouter> rest_router,
        std::shared_ptr<RequestRouter> request_router);

    ~QodRestAdapter() = default;

    /**
     * @brief Register all CAMARA QoD endpoints with the HTTP service
     *
     * @param http_service HTTP communication service to register endpoints with
     */
    void register_endpoints(
        af::communication::CommunicationServicePtr http_service);

private:
    /**
     * @brief Create a handler for a specific endpoint
     *
     * @param message_type Internal message type
     * @param path_pattern Path pattern for parameter extraction
     * @return HTTP request handler
     */
    af::communication::HttpRequestHandler create_handler(
        const std::string& message_type,
        const std::string& path_pattern = "");

    std::shared_ptr<rest::RestRouter> rest_router_;
    std::shared_ptr<RequestRouter> request_router_;
};

} // namespace qod
} // namespace core
} // namespace af
