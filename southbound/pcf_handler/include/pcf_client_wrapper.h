/**
 * @file pcf_client_wrapper.h
 * @brief Wrapper for the PCF client API
 *
 * This component provides a C++ wrapper around the PCF HTTP/2 API client.
 * It handles the low-level HTTP/2 communication with the 5G PCF.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <nghttp2/nghttp2.h>
#include <boost/asio.hpp>

namespace af {
namespace southbound {

// TODO: Resolve issue with connection reuse, currently a new connection is made for each request

/**
 * @brief Wrapper for the PCF client API
 */
class PcfClientWrapper {
public:
    /**
     * @brief Constructor
     * @param base_url Base URL for the PCF API
     * @param use_tls Whether to use TLS for the connection
     * @param api_version API version to use
     */
    PcfClientWrapper(const std::string& base_url, 
                     bool use_tls = false,
                     const std::string& api_version = "v1");
    
    /**
     * @brief Destructor
     */
    ~PcfClientWrapper();
    
    /**
     * @brief Initialize the PCF client
     * @return true if initialization succeeded
     */
    bool initialize();
    
    /**
     * @brief Create an application session
     * @param app_session_context Application session context
     * @return Pair of success flag and response data
     */
    std::pair<bool, nlohmann::json> create_app_session(
        const nlohmann::json& app_session_context);
    
    /**
     * @brief Update an application session
     * @param app_session_id Application session ID
     * @param update_data Update data
     * @return Pair of success flag and response data
     */
    std::pair<bool, nlohmann::json> update_app_session(
        const std::string& app_session_id,
        const nlohmann::json& update_data);
    
    /**
     * @brief Delete an application session
     * @param app_session_id Application session ID
     * @return true if deletion succeeded
     */
    bool delete_app_session(const std::string& app_session_id, const nlohmann::json& delete_data);
    
    /**
     * @brief Get information about an application session
     * @param app_session_id Application session ID
     * @return Pair of success flag and response data
     */
    std::pair<bool, nlohmann::json> get_app_session(
        const std::string& app_session_id);

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // PCF API configuration
    std::string base_url_;
    bool use_tls_;
    std::string api_version_;
    std::string host_;
    std::string port_;
    std::string path_prefix_;
    
    // HTTP/2 client session
    nghttp2_session* session_;
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_;
    
    // Connection state
    bool connected_;
    bool connection_error_; // Track if connection had an error
    std::mutex session_mutex_;
    
    // Response data
    struct ResponseData {
        int status_code;
        std::map<std::string, std::string> headers;
        std::string body;
        bool completed;
    };
    
    std::map<int32_t, ResponseData> responses_;
    std::mutex responses_mutex_;
    
    // Logger
    std::shared_ptr<spdlog::logger> logger_;
    
    /**
     * @brief Initialize nghttp2 for HTTP/2
     * @return true if initialization succeeded
     */
    bool initialize_nghttp2();
    
    /**
     * @brief Connect to the PCF server
     * @return true if connection succeeded
     */
    bool connect();
    
    /**
     * @brief Disconnect from the PCF server
     */
    void disconnect();
    
    /**
     * @brief Check if connection is still alive
     * @return true if connection is alive
     */
    bool is_connection_alive();
    
    /**
     * @brief Parse the URL into host, port, and path
     * @param url URL to parse
     * @return true if parsing succeeded
     */
    bool parse_url(const std::string& url);
    
    /**
     * @brief Perform an HTTP request
     * @param method HTTP method (GET, POST, PUT, DELETE)
     * @param path Path to request
     * @param request_data Request data (for POST/PUT)
     * @return Pair of success flag and response data
     */
    std::pair<bool, nlohmann::json> perform_request(
        const std::string& method,
        const std::string& path,
        const nlohmann::json& request_data = nlohmann::json());
    
    /**
     * @brief Submit a request to the session
     * @param method HTTP method
     * @param path Request path
     * @param headers HTTP headers
     * @param body Request body (for POST/PUT/PATCH)
     * @return Stream ID or -1 on error
     */
    int32_t submit_request(
        const std::string& method,
        const std::string& path,
        const std::map<std::string, std::string>& headers,
        const std::string& body);
    
    /**
     * @brief Wait for response completion
     * @param stream_id Stream ID to wait for
     * @param timeout_ms Timeout in milliseconds
     * @return true if response completed within timeout
     */
    bool wait_for_response(int32_t stream_id, int timeout_ms = 5000);
    
    /**
     * @brief Process received data
     * @param data Data buffer
     * @param length Data length
     * @return Number of bytes processed
     */
    ssize_t process_data(const uint8_t* data, size_t length);
    
    /**
     * @brief Frame receive callback for nghttp2
     */
    static int on_frame_recv_callback(nghttp2_session *session,
                                     const nghttp2_frame *frame,
                                     void *user_data);
    
    /**
     * @brief Header callback for nghttp2
     */
    static int on_header_callback(nghttp2_session *session,
                                 const nghttp2_frame *frame,
                                 const uint8_t *name, size_t namelen,
                                 const uint8_t *value, size_t valuelen,
                                 uint8_t flags, void *user_data);
    
    /**
     * @brief Data chunk callback for nghttp2
     */
    static int on_data_chunk_recv_callback(nghttp2_session *session,
                                          uint8_t flags, int32_t stream_id,
                                          const uint8_t *data, size_t len,
                                          void *user_data);
    
    /**
     * @brief Stream close callback for nghttp2
     */
    static int on_stream_close_callback(nghttp2_session *session,
                                       int32_t stream_id, uint32_t error_code,
                                       void *user_data);
};

} // namespace southbound
} // namespace af