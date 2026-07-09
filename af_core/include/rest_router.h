/**
 * @file rest_router.h
 * @brief Generic REST to internal Message converter
 * 
 * This component provides utilities for converting between REST HTTP requests
 * and internal AF Message objects. It handles:
 * - Path parameter extraction (e.g., {sessionId})
 * - HTTP headers to Message.metadata mapping
 * - HTTP body to Message.payload conversion
 * - Message response to HTTP response conversion
 */

#pragma once

#include "communication_interface.h"
#include <string>
#include <map>
#include <regex>
#include <nlohmann/json.hpp>

namespace af {
namespace core {
namespace rest {

/**
 * @brief Converts between REST HTTP and internal Message format
 */
class RestRouter {
public:
    RestRouter() = default;
    ~RestRouter() = default;

    /**
     * @brief Convert HTTP request to internal Message format
     * 
     * @param req HTTP request
     * @param message_type Internal message type to set
     * @param path_pattern Optional path pattern for extracting path parameters
     * @return MessagePtr Internal message
     */
    af::communication::MessagePtr http_request_to_message(
        const af::communication::HttpRequest& req,
        const std::string& message_type,
        const std::string& path_pattern = "");

    /**
     * @brief Convert Message response to HTTP response
     * 
     * @param msg Internal message response
     * @return HttpServerResponse HTTP response
     */
    af::communication::HttpServerResponse message_to_http_response(
        const af::communication::MessagePtr& msg);

    /**
     * @brief Extract path parameters from a path using a pattern
     * 
     * Extracts parameters from paths like:
     *   pattern: "/sessions/{sessionId}/extend"
     *   path:    "/sessions/123e4567-e89b-12d3-a456-426614174000/extend"
     *   result:  {"sessionId": "123e4567-e89b-12d3-a456-426614174000"}
     * 
     * @param pattern Path pattern with {param} placeholders
     * @param path Actual path
     * @return Map of parameter names to values
     */
    std::map<std::string, std::string> extract_path_params(
        const std::string& pattern,
        const std::string& path);

private:
    /**
     * @brief Generate correlation ID from request or create new one
     */
    std::string get_correlation_id(const af::communication::HttpRequest& req);

    /**
     * @brief Convert path pattern to regex for matching
     */
    std::string pattern_to_regex(const std::string& pattern);
};

} // namespace rest
} // namespace core
} // namespace af
