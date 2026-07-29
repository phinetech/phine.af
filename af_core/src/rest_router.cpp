/**
 * @file rest_router.cpp
 * @brief Implementation of the REST router
 */

#include "rest_router.h"
#include "path_pattern.h"

#include <boost/url.hpp>
#include <sstream>
#include <random>
#include <iomanip>

namespace af {
namespace core {
namespace rest {

af::communication::MessagePtr RestRouter::http_request_to_message(
    const af::communication::HttpRequest& req,
    const std::string& message_type,
    const std::string& path_pattern) {

    auto message = std::make_shared<af::communication::Message>();

    // Set message type and correlation ID
    message->message_type = message_type;
    message->correlation_id = get_correlation_id(req);

    // Convert HTTP body to payload
    message->payload = std::vector<uint8_t>(req.body.begin(), req.body.end());

    // Copy HTTP headers to metadata
    for (const auto& [key, value] : req.headers) {
        // Convert header names to lowercase for consistency
        std::string lowercase_key = key;
        std::transform(lowercase_key.begin(), lowercase_key.end(),
                      lowercase_key.begin(), ::tolower);
        message->metadata[lowercase_key] = value;
    }

    // Extract path parameters if pattern provided.
    // Pass only the path component (no query string) to extract_path_params so
    // that the regex anchored on '$' still matches correctly.
    if (!path_pattern.empty()) {
        std::string path_only = req.path;
        {
            auto parsed = boost::urls::parse_relative_ref(req.path);
            if (parsed) {
                path_only = parsed->path();
            }
        }
        auto params = extract_path_params(path_pattern, path_only);
        for (const auto& [param_name, param_value] : params) {
            message->metadata[param_name] = param_value;
        }
    }

    // Extract query parameters — stored as "query.<name>" to avoid collisions
    // with path params and header names in the same metadata map.
    auto query_params = extract_query_params(req.path);
    for (const auto& [k, v] : query_params) {
        message->metadata[k] = v;
    }

    // Store HTTP method and path for reference
    message->metadata["http_method"] = req.method;
    message->metadata["http_path"] = req.path;

    return message;
}

af::communication::HttpServerResponse RestRouter::message_to_http_response(
    const af::communication::MessagePtr& msg) {

    af::communication::HttpServerResponse response;

    if (!msg) {
        response.status_code = 500;
        response.body = R"({"error": "Internal server error"})";
        response.headers["content-type"] = "application/json";
        return response;
    }

    // Extract status code from metadata (default to 200)
    auto status_it = msg->metadata.find("status");
    if (status_it != msg->metadata.end()) {
        try {
            response.status_code = std::stoi(status_it->second);
        } catch (...) {
            response.status_code = 200;
        }
    } else {
        response.status_code = 200;
    }

    // Convert payload to response body
    response.body = std::string(msg->payload.begin(), msg->payload.end());

    // Copy metadata to HTTP headers (excluding internal fields)
    for (const auto& [key, value] : msg->metadata) {
        // Skip internal metadata fields
        if (key == "status" || key == "http_method" || key == "http_path") {
            continue;
        }
        response.headers[key] = value;
    }

    // Ensure content-type is set
    if (response.headers.find("content-type") == response.headers.end()) {
        response.headers["content-type"] = "application/json";
    }

    // Add correlation ID header if present
    if (!msg->correlation_id.empty()) {
        response.headers["x-correlator"] = msg->correlation_id;
    }

    return response;
}

std::map<std::string, std::string> RestRouter::extract_path_params(
    const std::string& pattern,
    const std::string& path) {

    std::map<std::string, std::string> params;

    // Build anchored regex from the pattern using the shared utility.
    const std::string regex_str =
        af::communication::http::path_pattern::to_regex(pattern);
    const std::regex path_regex("^" + regex_str + "$");
    std::smatch matches;

    if (!std::regex_match(path, matches, path_regex)) {
        return params;
    }

    // Extract parameter names from {param} tokens in the pattern.
    std::vector<std::string> param_names;
    static const std::regex param_name_re(R"(\{([^}]+)\})");
    auto it  = std::sregex_iterator(pattern.begin(), pattern.end(), param_name_re);
    auto end = std::sregex_iterator();
    for (; it != end; ++it) {
        param_names.push_back((*it)[1].str());
    }

    // Correlate captured groups with param names.
    // matches[0] is the full match; captures start at index 1.
    // Percent-decode each captured value via boost::urls.
    for (std::size_t i = 0;
         i < param_names.size() && i + 1 < matches.size();
         ++i) {
        const std::string raw = matches[i + 1].str();
        std::string decoded;
        boost::urls::pct_string_view pct(raw);
        decoded = pct.decode();
        params[param_names[i]] = std::move(decoded);
    }

    return params;
}

std::string RestRouter::get_correlation_id(const af::communication::HttpRequest& req) {
    // Try to get x-correlator header
    auto it = req.headers.find("x-correlator");
    if (it != req.headers.end() && !it->second.empty()) {
        return it->second;
    }

    // Try lowercase variant
    it = req.headers.find("X-Correlator");
    if (it != req.headers.end() && !it->second.empty()) {
        return it->second;
    }

    // Generate a new UUID-like correlation ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }

    return ss.str();
}

std::map<std::string, std::string> RestRouter::extract_query_params(
    const std::string& raw_path) {

    std::map<std::string, std::string> params;

    auto parsed = boost::urls::parse_relative_ref(raw_path);
    if (!parsed) {
        return params;
    }

    // boost::urls::url_view::params() iterates key-value pairs and
    // percent-decodes both key and value automatically.
    for (auto param : parsed->params()) {
        params["query." + std::string(param.key)] =
            std::string(param.value);
    }

    return params;
}

std::string RestRouter::pattern_to_regex(const std::string& pattern) {
    return af::communication::http::path_pattern::to_regex(pattern);
}

} // namespace rest
} // namespace core
} // namespace af
