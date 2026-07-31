/**
 * @file http_pcf_gateway.h
 * @brief HTTP/2-based implementation of the PcfGateway interface
 *
 * Uses HttpCommunicationService::send_http() to make direct HTTP/2 calls
 * to the 5G PCF Npcf_PolicyAuthorization API. This replaces the legacy
 * PcfClientWrapper with a DI-managed, testable implementation.
 */

#pragma once

#include "pcf_gateway.h"
#include "http_communication_service.h"

#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace af {
namespace southbound {

/**
 * @brief HTTP/2-based PCF gateway implementation
 *
 * Maps PCF operations to HTTP/2 requests via the shared HttpCommunicationService.
 *
 * Route mapping:
 *   create_app_session  → POST   /npcf-policyauthorization/{version}/app-sessions
 *   update_app_session  → PATCH  /npcf-policyauthorization/{version}/app-sessions/{id}
 *   delete_app_session  → POST   /npcf-policyauthorization/{version}/app-sessions/{id}/delete
 *   get_app_session     → GET    /npcf-policyauthorization/{version}/app-sessions/{id}
 */
class HttpPcfGateway : public PcfGateway {
public:
    /**
     * @brief Constructor
     * @param http_service The HTTP communication service for PCF SBI calls
     * @param api_version API version string (e.g., "v1")
     */
    HttpPcfGateway(std::shared_ptr<af::communication::http::HttpCommunicationService> http_service,
                   const std::string& api_version = "v1");

    ~HttpPcfGateway() override = default;

    bool initialize() override;

    std::pair<bool, nlohmann::json> create_app_session(
        const nlohmann::json& app_session_context) override;

    std::pair<bool, nlohmann::json> update_app_session(
        const std::string& app_session_id,
        const nlohmann::json& update_data) override;

    bool delete_app_session(
        const std::string& app_session_id,
        const nlohmann::json& delete_data) override;

    std::pair<bool, nlohmann::json> get_app_session(
        const std::string& app_session_id) override;

private:
    std::shared_ptr<af::communication::http::HttpCommunicationService> http_service_;
    std::string api_version_;
    std::string base_path_;  // e.g., "/npcf-policyauthorization/v1"
    std::shared_ptr<spdlog::logger> logger_;

    /**
     * @brief Build the base path from the API version
     */
    void build_base_path();

    /**
     * @brief Get common headers for PCF requests
     */
    std::map<std::string, std::string> get_common_headers() const;

    /**
     * @brief Parse a successful HTTP response body into JSON and attach the
     *        response metadata callers rely on.
     *
     * The returned object always carries a "headers" object (HTTP/2 header names
     * are lowercase, e.g. "location") and an "http_code" field, so callers can
     * extract values such as the app session id from the Location header. This
     * mirrors the contract the legacy PcfClientWrapper exposed.
     */
    nlohmann::json build_response_json(
        const af::communication::http::HttpResponse& response) const;
};

} // namespace southbound
} // namespace af
