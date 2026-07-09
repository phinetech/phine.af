/**
 * @file rest_router.cpp
 * @brief Implementation of the REST router
 */

#include "rest_router.h"
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

    // Extract path parameters if pattern provided
    if (!path_pattern.empty()) {
        auto params = extract_path_params(path_pattern, req.path);
        for (const auto& [param_name, param_value] : params) {
            message->metadata[param_name] = param_value;
        }
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

    // Convert pattern to regex
    std::string regex_str = pattern_to_regex(pattern);
    std::regex path_regex(regex_str);
    std::smatch matches;

    if (!std::regex_match(path, matches, path_regex)) {
        return params;
    }

    // Extract parameter names from pattern
    std::vector<std::string> param_names;
    std::regex param_regex(R"(\{([^}]+)\})");
    auto pattern_begin = std::sregex_iterator(pattern.begin(), pattern.end(), param_regex);
    auto pattern_end = std::sregex_iterator();

    for (auto it = pattern_begin; it != pattern_end; ++it) {
        param_names.push_back((*it)[1].str());
    }

    // Match parameter values
    for (size_t i = 0; i < param_names.size() && i + 1 < matches.size(); ++i) {
        params[param_names[i]] = matches[i + 1].str();
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

std::string RestRouter::pattern_to_regex(const std::string& pattern) {
    std::string regex_str = pattern;

    // Escape special regex characters except {}
    std::string escaped;
    for (char c : regex_str) {
        if (c == '.' || c == '+' || c == '*' || c == '?' ||
            c == '^' || c == '$' || c == '(' || c == ')' ||
            c == '[' || c == ']' || c == '|' || c == '\\') {
            escaped += '\\';
        }
        escaped += c;
    }

    // Replace {param} with ([^/]+) to capture path segments
    std::regex param_regex(R"(\\\{[^}]+\\\})");
    regex_str = std::regex_replace(escaped, param_regex, "([^/]+)");

    return regex_str;
}

} // namespace rest
} // namespace core
} // namespace af
