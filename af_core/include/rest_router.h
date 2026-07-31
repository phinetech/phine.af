/**
 * @file rest_router.h
 * @brief Generic REST to internal Message converter
 *
 * This component provides utilities for converting between REST HTTP requests
 * and internal AF Message objects. It handles:
 * - Path parameter extraction (e.g., {sessionId}), percent-decoded via boost::urls
 * - Query parameter extraction (e.g., ?status=ACTIVE), stored as "query.<name>"
 * - HTTP headers to Message.metadata mapping
 * - HTTP body to Message.payload conversion
 * - Message response to HTTP response conversion
 *
 * Metadata key conventions in the resulting Message:
 *   Path params  : "<paramName>"           e.g. "sessionId"
 *   Query params : "query.<name>"          e.g. "query.status"
 *   Headers      : "<lowercase-name>"      e.g. "content-type"
 *   HTTP method  : "http_method"
 *   HTTP path    : "http_path"             (full raw :path including query string)
 */

#pragma once

#include "communication_interface.h"
#include <string>
#include <map>
#include <regex>

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
     * @brief Extract path parameters from a path using a pattern.
     *
     * Parameter values are percent-decoded (e.g. "%2F" → "/").
     *
     * Example:
     *   pattern: "/sessions/{sessionId}/extend"
     *   path:    "/sessions/123e4567-e89b-12d3-a456-426614174000/extend"
     *   result:  {"sessionId": "123e4567-e89b-12d3-a456-426614174000"}
     *
     * @param pattern Path pattern with {param} or * placeholders
     * @param path    Actual path (path component only, no query string)
     * @return Map of parameter names to percent-decoded values
     */
    std::map<std::string, std::string> extract_path_params(
        const std::string& pattern,
        const std::string& path);

    /**
     * @brief Extract query parameters from a raw request path (or query string).
     *
     * Parses the query component of @p raw_path and returns each key-value pair
     * with its key prefixed by "query." to avoid collisions with path params and
     * headers in message->metadata.
     *
     * Keys and values are percent-decoded via boost::urls.
     *
     * Example:
     *   raw_path: "/sessions?status=ACTIVE&maxResults=50"
     *   result:   {"query.status": "ACTIVE", "query.maxResults": "50"}
     *
     * Repeated keys (e.g. "?tag=a&tag=b") are not currently supported by the
     * metadata map; the last value wins.
     *
     * @param raw_path Full raw :path pseudo-header value (may include query string)
     * @return Map of "query.<name>" to percent-decoded values
     */
    std::map<std::string, std::string> extract_query_params(
        const std::string& raw_path);

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
