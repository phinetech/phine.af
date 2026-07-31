/**
 * @file http_pcf_gateway.cpp
 * @brief Implementation of the HTTP/2-based PCF gateway
 */

#include "http_pcf_gateway.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace af {
namespace southbound {

HttpPcfGateway::HttpPcfGateway(
    std::shared_ptr<af::communication::http::HttpCommunicationService> http_service,
    const std::string& api_version)
    : http_service_(std::move(http_service))
    , api_version_(api_version) {

    // Setup logger
    logger_ = spdlog::get("http_pcf_gateway");
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("http_pcf_gateway");
    }

    build_base_path();
}

bool HttpPcfGateway::initialize() {
    logger_->info("Initializing HTTP PCF Gateway (base_path: {})", base_path_);

    if (!http_service_) {
        logger_->error("HTTP service is null");
        return false;
    }

    return true;
}

std::pair<bool, nlohmann::json> HttpPcfGateway::create_app_session(
    const nlohmann::json& app_session_context) {

    logger_->debug("Creating app session via HTTP/2");

    const std::string path = base_path_ + "/app-sessions";
    const std::string body = app_session_context.dump();
    auto headers = get_common_headers();

    logger_->trace("create_app_session: POST {} body={}", path, body);

    try {
        auto response = http_service_->send_http("POST", path, headers, body);

        logger_->trace("create_app_session: response error={} status={} body_len={}",
                       response.error, response.status_code, response.body.size());

        if (response.error) {
            logger_->error("HTTP request failed: {}", response.error_message);
            return {false, nlohmann::json{{"error", response.error_message}}};
        }

        // 2xx is success for create (typically 201 Created)
        if (response.status_code >= 200 && response.status_code < 300) {
            nlohmann::json response_json = build_response_json(response);
            logger_->info("App session created successfully (status: {})", response.status_code);
            return {true, response_json};
        } else {
            logger_->error("PCF returned error status: {} (body: '{}')",
                           response.status_code,
                           response.body.empty() ? "<empty>" : response.body);
            if (response.status_code == 0) {
                logger_->error("create_app_session: status 0 with no transport error usually "
                               "means the PCF reset the HTTP/2 stream (RST_STREAM) or sent "
                               "GOAWAY — check the http2_client logs for the error_code");
            }
            nlohmann::json error_json;
            if (!response.body.empty()) {
                error_json = nlohmann::json::parse(response.body, nullptr, false);
                if (error_json.is_discarded()) {
                    error_json = nlohmann::json{{"status", response.status_code}, {"detail", response.body}};
                }
            } else {
                error_json = nlohmann::json{{"status", response.status_code}};
            }
            return {false, error_json};
        }
    } catch (const std::exception& e) {
        logger_->error("Exception during create_app_session: {}", e.what());
        return {false, nlohmann::json{{"error", e.what()}}};
    }
}

std::pair<bool, nlohmann::json> HttpPcfGateway::update_app_session(
    const std::string& app_session_id,
    const nlohmann::json& update_data) {

    logger_->debug("Updating app session {} via HTTP/2", app_session_id);

    const std::string path = base_path_ + "/app-sessions/" + app_session_id;
    const std::string body = update_data.dump();
    auto headers = get_common_headers();

    try {
        auto response = http_service_->send_http("PATCH", path, headers, body);

        if (response.error) {
            logger_->error("HTTP request failed: {}", response.error_message);
            return {false, nlohmann::json{{"error", response.error_message}}};
        }

        if (response.status_code >= 200 && response.status_code < 300) {
            nlohmann::json response_json = build_response_json(response);
            logger_->info("App session {} updated successfully", app_session_id);
            return {true, response_json};
        } else {
            logger_->error("PCF returned error status: {} for update", response.status_code);
            nlohmann::json error_json;
            if (!response.body.empty()) {
                error_json = nlohmann::json::parse(response.body, nullptr, false);
                if (error_json.is_discarded()) {
                    error_json = nlohmann::json{{"status", response.status_code}, {"detail", response.body}};
                }
            } else {
                error_json = nlohmann::json{{"status", response.status_code}};
            }
            return {false, error_json};
        }
    } catch (const std::exception& e) {
        logger_->error("Exception during update_app_session: {}", e.what());
        return {false, nlohmann::json{{"error", e.what()}}};
    }
}

bool HttpPcfGateway::delete_app_session(
    const std::string& app_session_id,
    const nlohmann::json& delete_data) {

    logger_->debug("Deleting app session {} via HTTP/2", app_session_id);

    const std::string path = base_path_ + "/app-sessions/" + app_session_id + "/delete";
    const std::string body = delete_data.empty() ? "" : delete_data.dump();
    auto headers = get_common_headers();

    try {
        auto response = http_service_->send_http("POST", path, headers, body);

        if (response.error) {
            logger_->error("HTTP request failed: {}", response.error_message);
            return false;
        }

        // 2xx or 204 No Content is success for delete
        if (response.status_code >= 200 && response.status_code < 300) {
            logger_->info("App session {} deleted successfully", app_session_id);
            return true;
        } else {
            logger_->error("PCF returned error status: {} for delete", response.status_code);
            return false;
        }
    } catch (const std::exception& e) {
        logger_->error("Exception during delete_app_session: {}", e.what());
        return false;
    }
}

std::pair<bool, nlohmann::json> HttpPcfGateway::get_app_session(
    const std::string& app_session_id) {

    logger_->debug("Getting app session {} via HTTP/2", app_session_id);

    const std::string path = base_path_ + "/app-sessions/" + app_session_id;
    auto headers = get_common_headers();

    try {
        auto response = http_service_->send_http("GET", path, headers, "");

        if (response.error) {
            logger_->error("HTTP request failed: {}", response.error_message);
            return {false, nlohmann::json{{"error", response.error_message}}};
        }

        if (response.status_code >= 200 && response.status_code < 300) {
            nlohmann::json response_json = build_response_json(response);
            logger_->info("Got app session {} successfully", app_session_id);
            return {true, response_json};
        } else {
            logger_->error("PCF returned error status: {} for get", response.status_code);
            nlohmann::json error_json;
            if (!response.body.empty()) {
                error_json = nlohmann::json::parse(response.body, nullptr, false);
                if (error_json.is_discarded()) {
                    error_json = nlohmann::json{{"status", response.status_code}, {"detail", response.body}};
                }
            } else {
                error_json = nlohmann::json{{"status", response.status_code}};
            }
            return {false, error_json};
        }
    } catch (const std::exception& e) {
        logger_->error("Exception during get_app_session: {}", e.what());
        return {false, nlohmann::json{{"error", e.what()}}};
    }
}

void HttpPcfGateway::build_base_path() {
    base_path_ = "/npcf-policyauthorization/" + api_version_;
    logger_->debug("PCF gateway base path: {}", base_path_);
}

std::map<std::string, std::string> HttpPcfGateway::get_common_headers() const {
    return {
        {"content-type", "application/json"},
        {"accept", "application/json"}
    };
}

nlohmann::json HttpPcfGateway::build_response_json(
    const af::communication::http::HttpResponse& response) const {

    nlohmann::json response_json;
    if (!response.body.empty()) {
        response_json = nlohmann::json::parse(response.body, nullptr, false);
        if (response_json.is_discarded()) {
            logger_->warn("Failed to parse response body as JSON");
            response_json = nlohmann::json{{"raw_body", response.body}};
        }
    }

    // Ensure we have an object we can attach metadata to without clobbering the
    // parsed body (handles empty/array/scalar bodies).
    if (!response_json.is_object()) {
        response_json = nlohmann::json{{"body", response_json}};
    }

    // Attach response headers so callers can read e.g. the Location header to
    // extract the app session id. nghttp2 delivers HTTP/2 header names in
    // lowercase ("location"); callers check both "Location" and "location".
    nlohmann::json headers_json = nlohmann::json::object();
    for (const auto& [name, value] : response.headers) {
        headers_json[name] = value;
    }
    response_json["headers"] = headers_json;
    response_json["http_code"] = response.status_code;

    return response_json;
}

} // namespace southbound
} // namespace af
