/**
 * @file pcf_client_wrapper.cpp
 * @brief Implementation of the PCF client wrapper using nghttp2
 */

#include "pcf_client_wrapper.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>
#include <thread>
#include <chrono>
#include <regex>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>

namespace af {
namespace southbound {

PcfClientWrapper::PcfClientWrapper(const std::string& base_url,
                                 bool use_tls,
                                 const std::string& api_version)
    : base_url_(base_url), use_tls_(use_tls), api_version_(api_version),
      session_(nullptr), socket_(io_context_), connected_(false), connection_error_(false) {

    // Setup logger
    initializeLogger(spdlog::level::debug);

    logger_->info("PCF Client Wrapper created");

    // Parse base URL
    if (!parse_url(base_url_)) {
        logger_->error("Failed to parse base URL: {}", base_url_);
    }

    // Initialize nghttp2 session
    if (!initialize_nghttp2()) {
        logger_->error("Failed to initialize nghttp2 session");
    }
}

PcfClientWrapper::~PcfClientWrapper() {
    // Clean up nghttp2 resources
    if (session_) {
        nghttp2_session_del(session_);
        session_ = nullptr;
    }

    // Close socket if connected
    if (connected_ && socket_.is_open()) {
        boost::system::error_code ec;
        socket_.close(ec);
    }

    logger_->info("PCF Client Wrapper destroyed");
}

void PcfClientWrapper::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("af_pcf_client");

    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("af_pcf_client");
    }

    // Set the log level
    logger_->set_level(log_level);

    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

bool PcfClientWrapper::initialize() {
    logger_->info("Initializing PCF client");

    // Initialize nghttp2
    if (!initialize_nghttp2()) {
        logger_->error("Failed to initialize nghttp2");
        return false;
    }

    logger_->info("PCF client initialized successfully");
    return true;
}

bool PcfClientWrapper::parse_url(const std::string& url) {
    // Parse URL using regex
    std::regex url_regex("(https?)://([^:/]+)(?::([0-9]+))?(/.*)?");
    std::smatch matches;

    if (!std::regex_match(url, matches, url_regex)) {
        logger_->error("Invalid URL format: {}", url);
        return false;
    }

    std::string protocol = matches[1].str();
    host_ = matches[2].str();
    port_ = matches[3].matched ? matches[3].str() : (protocol == "https" ? "443" : "80");
    path_prefix_ = matches[4].matched ? matches[4].str() : "/";

    // Ensure path prefix ends with /
    if (!path_prefix_.empty() && path_prefix_.back() != '/') {
        path_prefix_ += '/';
    }

    logger_->debug("URL parsed - Host: {}, Port: {}, Path: {}",
                  host_, port_, path_prefix_);
    return true;
}

bool PcfClientWrapper::initialize_nghttp2() {
    // Define callbacks
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);

    // Set callbacks
    nghttp2_session_callbacks_set_on_frame_recv_callback(
        callbacks, on_frame_recv_callback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
        callbacks, on_data_chunk_recv_callback);
    nghttp2_session_callbacks_set_on_header_callback(
        callbacks, on_header_callback);
    nghttp2_session_callbacks_set_on_stream_close_callback(
        callbacks, on_stream_close_callback);

    // Create a client session
    nghttp2_session_client_new(&session_, callbacks, this);

    // Free callbacks as they're now copied into the session
    nghttp2_session_callbacks_del(callbacks);

    // Submit the client connection header (SETTINGS frame)
    nghttp2_settings_entry iv[1] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}
    };

    nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv,
                           sizeof(iv) / sizeof(iv[0]));

    return true;
}

bool PcfClientWrapper::connect() {
    if (connected_ && is_connection_alive()) {
        return true;
    }

    logger_->debug("Connecting to {}:{} with HTTP/2 prior knowledge", host_, port_);

    const int max_connect_attempts = 3;
    const int base_delay_ms = 50;

    for (int attempt = 0; attempt < max_connect_attempts; attempt++) {
        if (attempt > 0) {
            int delay_ms = base_delay_ms * (1 << (attempt - 1)); // 50ms, 100ms, 200ms
            logger_->debug("Retrying connection (attempt {}/{}) after {} ms delay",
                         attempt + 1, max_connect_attempts, delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }

        try {
            // Ensure socket is closed from previous attempts
            if (socket_.is_open()) {
                boost::system::error_code ec;
                socket_.close(ec);
            }

            // CRITICAL: Verify session is initialized
            if (session_ == nullptr) {
                logger_->error("nghttp2 session is not initialized. Call initialize_nghttp2() first.");
                if (attempt == max_connect_attempts - 1) {
                    return false;
                }
                initialize_nghttp2(); // Try to reinitialize
                continue;
            }

            // Resolve the host with timeout
            boost::asio::ip::tcp::resolver resolver(io_context_);
            boost::asio::ip::tcp::resolver::results_type endpoints;

            try {
                endpoints = resolver.resolve(host_, port_);
                logger_->debug("Resolved endpoints on attempt {}", attempt + 1);
            } catch (const std::exception& e) {
                logger_->error("DNS resolution failed on attempt {}: {}", attempt + 1, e.what());
                if (attempt == max_connect_attempts - 1) {
                    return false;
                }
                continue;
            }

            // Connect to the host with timeout
            boost::system::error_code connect_ec;
            auto connect_result = boost::asio::connect(socket_, endpoints, connect_ec);

            if (connect_ec) {
                logger_->error("TCP connection failed on attempt {}: {}", attempt + 1, connect_ec.message());
                if (attempt == max_connect_attempts - 1) {
                    return false;
                }
                continue;
            }

            logger_->debug("TCP connection established on attempt {}", attempt + 1);

            // Set socket options for better reliability
            boost::asio::socket_base::keep_alive keep_alive_option(true);
            socket_.set_option(keep_alive_option);

            boost::asio::ip::tcp::no_delay no_delay_option(true);
            socket_.set_option(no_delay_option);

            // Configure proper HTTP/2 settings
            nghttp2_settings_entry iv[] = {
                {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
                {NGHTTP2_SETTINGS_ENABLE_PUSH, 0},            // Disable server push
                {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 65535}, // Default window size
                {NGHTTP2_SETTINGS_MAX_FRAME_SIZE, 16384}      // Default frame size
            };

            logger_->debug("Submitting HTTP/2 SETTINGS frame");
            int rv = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv,
                                             sizeof(iv) / sizeof(iv[0]));
            if (rv != 0) {
                logger_->error("nghttp2_submit_settings failed on attempt {}: {}",
                             attempt + 1, nghttp2_strerror(rv));
                if (attempt == max_connect_attempts - 1) {
                    return false;
                }
                continue;
            }

            logger_->debug("Sending HTTP/2 SETTINGS frame");
            // Send the SETTINGS frame
            std::vector<uint8_t> buffer(16384);
            const uint8_t* data_ptr;
            ssize_t serlen = nghttp2_session_mem_send(session_, &data_ptr);

            if (serlen > 0) {
                boost::system::error_code write_ec;
                boost::asio::write(socket_, boost::asio::buffer(data_ptr, serlen), write_ec);
                if (write_ec) {
                    logger_->error("Failed to send SETTINGS frame on attempt {}: {}",
                                 attempt + 1, write_ec.message());
                    if (attempt == max_connect_attempts - 1) {
                        return false;
                    }
                    continue;
                }
            }

            logger_->debug("HTTP/2 SETTINGS frame sent, waiting for server response");

            // Exchange frames to complete the handshake with timeout
            bool handshake_complete = false;
            int handshake_attempt = 0;
            auto handshake_start = std::chrono::steady_clock::now();
            const int handshake_timeout_ms = 3000; // 3 second timeout for handshake

            while (!handshake_complete && handshake_attempt < 5) {
                logger_->debug("Handshake attempt {}", handshake_attempt + 1);
                handshake_attempt++;

                // Check handshake timeout
                auto current_time = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - handshake_start).count();

                if (elapsed > handshake_timeout_ms) {
                    logger_->error("Handshake timeout after {} ms on connection attempt {}",
                                 elapsed, attempt + 1);
                    break;
                }

                try {
                    // Set socket timeout for read
                    socket_.non_blocking(false);

                    // Read from socket with timeout
                    buffer.resize(16384);
                    boost::system::error_code read_ec;
                    size_t readlen = socket_.read_some(boost::asio::buffer(buffer), read_ec);

                    if (read_ec) {
                        if (read_ec == boost::asio::error::would_block ||
                            read_ec == boost::asio::error::try_again) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            continue;
                        } else if (read_ec == boost::asio::error::eof) {
                            logger_->error("Server closed connection during handshake on attempt {}",
                                         attempt + 1);
                            break;
                        } else {
                            logger_->error("Read error during handshake on attempt {}: {}",
                                         attempt + 1, read_ec.message());
                            break;
                        }
                    }

                    logger_->debug("Read {} bytes from socket", readlen);
                    if (readlen > 0) {

                        // Process the data
                        ssize_t processlen = nghttp2_session_mem_recv(session_, buffer.data(), readlen);

                        if (processlen < 0) {
                            logger_->error("Error processing received data on attempt {}: {}",
                                           attempt + 1, nghttp2_strerror((int)processlen));
                            break;
                        }

                        // Debug the received frames
                        logger_->debug("Processed {} bytes of HTTP/2 frames", processlen);
                    }

                    // Send any pending data
                    while ((serlen = nghttp2_session_mem_send(session_, &data_ptr)) > 0) {
                        boost::system::error_code write_ec;
                        boost::asio::write(socket_, boost::asio::buffer(data_ptr, serlen), write_ec);
                        if (write_ec) {
                            logger_->error("Failed to send handshake data on attempt {}: {}",
                                         attempt + 1, write_ec.message());
                            break;
                        }
                    }

                    // If we've successfully exchanged SETTINGS frames, consider handshake complete
                    if (handshake_attempt >= 2) {
                        handshake_complete = true;
                    }
                }
                catch (const boost::system::system_error& e) {
                    if (e.code() == boost::asio::error::eof) {
                        logger_->error("Server closed connection during handshake on attempt {}",
                                     attempt + 1);
                        break;
                    }
                    logger_->error("Boost system error during handshake on attempt {}: {}",
                                 attempt + 1, e.what());
                    break;
                }
                // Catch other exceptions and log
                catch (const std::exception& e) {
                    logger_->error("Exception during handshake on attempt {}: {}",
                                 attempt + 1, e.what());
                    break;
                }
            }

            if (!handshake_complete) {
                logger_->error("Handshake failed on connection attempt {}", attempt + 1);
                if (attempt == max_connect_attempts - 1) {
                    return false;
                }
                continue;
            }

            connected_ = true;
            connection_error_ = false; // Reset error flag on successful connection
            logger_->info("HTTP/2 connection established successfully on attempt {}", attempt + 1);

            // Initialize response map for future requests
            responses_.clear();

            return true;
        }
        catch (const std::exception& e) {
            logger_->error("Connection failed on attempt {}: {}", attempt + 1, e.what());
            if (attempt == max_connect_attempts - 1) {
                return false;
            }
        }
    }

    logger_->error("Failed to establish connection after {} attempts", max_connect_attempts);
    return false;
}

void PcfClientWrapper::disconnect() {
    logger_->debug("Disconnecting from PCF server");

    // Close socket if connected
    if (socket_.is_open()) {
        boost::system::error_code ec;
        socket_.close(ec);
        if (ec) {
            logger_->warn("Error closing socket: {}", ec.message());
        }
    }

    // Reset connection state
    connected_ = false;
    connection_error_ = false;

    // Clear any pending responses
    {
        std::lock_guard<std::mutex> lock(responses_mutex_);
        responses_.clear();
    }

    // Reset nghttp2 session
    if (session_) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        nghttp2_session_del(session_);
        session_ = nullptr;
    }

    // Reinitialize nghttp2 for next connection
    initialize_nghttp2();

    logger_->debug("Disconnected from PCF server");
}

bool PcfClientWrapper::is_connection_alive() {
    if (!connected_ || !socket_.is_open() || !session_ || connection_error_) {
        return false;
    }

    // Check if nghttp2 session wants to terminate the connection
    if (nghttp2_session_want_read(session_) == 0 && nghttp2_session_want_write(session_) == 0) {
        logger_->debug("HTTP/2 session wants to terminate");
        return false;
    }

    try {
        // Try to check socket state without blocking
        boost::system::error_code ec;
        socket_.non_blocking(true, ec);
        if (ec) {
            logger_->warn("Failed to set socket non-blocking: {}", ec.message());
            socket_.non_blocking(false); // Try to reset
            return false;
        }

        // Try to peek at the socket to see if it's still connected
        char buffer[1];
        size_t bytes_read = socket_.receive(boost::asio::buffer(buffer, 1),
                                           boost::asio::socket_base::message_peek, ec);

        // Reset to blocking mode
        socket_.non_blocking(false);

        if (ec == boost::asio::error::would_block) {
            // No data available but connection is alive
            return true;
        } else if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset ||
                   ec == boost::asio::error::broken_pipe || ec == boost::asio::error::connection_aborted) {
            // Connection closed
            logger_->debug("Connection closed by peer: {}", ec.message());
            return false;
        } else if (ec) {
            // Other error - consider connection potentially bad
            logger_->warn("Socket error during connection check: {}", ec.message());
            return false;
        }

        // If we got here, there might be data available or connection is fine
        // For HTTP/2, having data available could mean:
        // 1. Valid response data
        // 2. Connection close frame
        // 3. Other control frames
        // We'll consider it alive and let the request handling deal with specifics
        return true;
    }
    catch (const std::exception& e) {
        logger_->error("Exception during connection health check: {}", e.what());
        return false;
    }
}

std::pair<bool, nlohmann::json> PcfClientWrapper::create_app_session(
    const nlohmann::json& app_session_context) {

    logger_->info("Creating application session with PCF");

    // Construct path
    std::string path = "app-sessions";

    // Perform POST request
    return perform_request("POST", path, app_session_context);
}

std::pair<bool, nlohmann::json> PcfClientWrapper::update_app_session(
    const std::string& app_session_id,
    const nlohmann::json& update_data) {

    logger_->info("Updating application session: {}", app_session_id);

    // Construct path
    std::string path = "app-sessions/" + app_session_id;

    // Perform PATCH request
    return perform_request("PATCH", path, update_data);
}

bool PcfClientWrapper::delete_app_session(const std::string& app_session_id, const nlohmann::json& delete_data) {
    logger_->info("Deleting application session: {}", app_session_id);

    // Construct path
    std::string path = "app-sessions/" + app_session_id + "/delete";

    // Perform DELETE request
    auto result = perform_request("POST", path, delete_data);
    return result.first;
}

std::pair<bool, nlohmann::json> PcfClientWrapper::get_app_session(
    const std::string& app_session_id) {

    logger_->info("Getting application session: {}", app_session_id);

    // Construct path
    std::string path = "app-sessions/" + app_session_id;

    // Perform GET request
    return perform_request("GET", path);
}

std::pair<bool, nlohmann::json> PcfClientWrapper::perform_request(
    const std::string& method,
    const std::string& path,
    const nlohmann::json& request_data) {

    std::string full_path = path_prefix_ + path;
    logger_->debug("Performing {} request to {}", method, full_path);
    logger_->debug("Full URL would be: {}://{}:{}{}",
                  use_tls_ ? "https" : "http", host_, port_, full_path);

    // Retry logic with exponential backoff
    const int max_retries = 3;
    const int base_delay_ms = 100;

    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (attempt > 0) {
            // Exponential backoff: 100ms, 200ms, 400ms
            int delay_ms = base_delay_ms * (1 << (attempt - 1));
            logger_->info("Retrying request (attempt {}/{}) after {} ms delay",
                         attempt + 1, max_retries, delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }

        // Ensure we're connected - reconnect if needed
        if (!connected_ || !is_connection_alive()) {
            logger_->debug("Connection not alive, reconnecting...");
            disconnect();
            if (!connect()) {
                if (attempt == max_retries - 1) {
                    logger_->error("Failed to establish connection after {} attempts", max_retries);
                    return {false, {{"error", "connection_failed"}, {"attempts", max_retries}}};
                }
                continue; // Try again
            }
        }

        // Set headers
        std::map<std::string, std::string> headers = {
            {":method", method},
            {":scheme", use_tls_ ? "https" : "http"},
            {":authority", host_ + ":" + port_},
            {":path", full_path},
            {"content-type", "application/json"},
            {"accept", "application/json"},
            {"user-agent", "AF-PCF-Client/1.0"}
        };

        // Prepare request body
        std::string request_body;
        if (!request_data.empty()) {
            request_body = request_data.dump();
            headers["content-length"] = std::to_string(request_body.length());
        }

        // Submit the request
        int32_t stream_id = submit_request(method, full_path, headers, request_body);

        if (stream_id < 0) {
            logger_->error("Failed to submit request on attempt {}", attempt + 1);
            if (attempt == max_retries - 1) {
                return {false, {{"error", "request_submission_failed"}, {"attempts", max_retries}}};
            }
            // Force reconnect on next attempt
            connection_error_ = true;
            continue;
        }

        // Wait for response with increased timeout for retries
        int timeout_ms = 5000 + (attempt * 2000); // 5s, 7s, 9s
        if (!wait_for_response(stream_id, timeout_ms)) {
            logger_->error("Request timed out for stream {} on attempt {}", stream_id, attempt + 1);

            // Clean up the response entry
            {
                std::lock_guard<std::mutex> lock(responses_mutex_);
                responses_.erase(stream_id);
            }

            if (attempt == max_retries - 1) {
                return {false, {{"error", "request_timeout"}, {"attempts", max_retries}, {"timeout_ms", timeout_ms}}};
            }
            // Force reconnect on next attempt
            connection_error_ = true;
            continue;
        }

        // Get response
        std::lock_guard<std::mutex> lock(responses_mutex_);
        auto it = responses_.find(stream_id);

        if (it == responses_.end() || !it->second.completed) {
            logger_->error("No response received for stream {} on attempt {}", stream_id, attempt + 1);
            if (attempt == max_retries - 1) {
                return {false, {{"error", "no_response"}, {"attempts", max_retries}}};
            }
            continue;
        }

        ResponseData& response = it->second;

        // Check for retryable HTTP status codes
        bool is_retryable = (response.status_code == 0 ||           // Connection error
                            response.status_code == 502 ||          // Bad Gateway
                            response.status_code == 503 ||          // Service Unavailable
                            response.status_code == 504 ||          // Gateway Timeout
                            response.status_code >= 500);           // Other server errors

        // Check HTTP status code
        bool success = (response.status_code >= 200 && response.status_code < 300);

        if (!success && is_retryable && attempt < max_retries - 1) {
            logger_->warn("Received retryable error {} on attempt {}, will retry",
                         response.status_code, attempt + 1);
            responses_.erase(it); // Clean up before retry
            connection_error_ = true; // Force reconnect
            continue;
        }

        try {
            // Parse response body if available
            nlohmann::json response_json;

            // Debug: Log the raw response for troubleshooting
            logger_->debug("Raw response body (first 200 chars): {}",
                         response.body.length() > 200 ? response.body.substr(0, 200) + "..." : response.body);
            logger_->debug("Response status code: {}", response.status_code);

            if (!response.body.empty()) {
                // Check if the response looks like JSON
                std::string trimmed_body = response.body;
                // Remove leading/trailing whitespace
                trimmed_body.erase(0, trimmed_body.find_first_not_of(" \t\n\r"));
                trimmed_body.erase(trimmed_body.find_last_not_of(" \t\n\r") + 1);

                if (!trimmed_body.empty() && (trimmed_body[0] == '{' || trimmed_body[0] == '[')) {
                    // Looks like JSON, try to parse
                    try {
                        response_json = nlohmann::json::parse(trimmed_body);
                    } catch (const nlohmann::json::parse_error& pe) {
                        logger_->warn("JSON parse failed: {}", pe.what());
                        logger_->warn("Raw response body: {}", response.body);
                        // Create a JSON object with the raw body as a string
                        response_json = nlohmann::json::object();
                        response_json["raw_body"] = response.body;
                        response_json["parse_error"] = pe.what();
                    }
                } else {
                    // Not JSON, treat as plain text response
                    logger_->warn("Response is not JSON format. Raw body: {}", response.body);
                    response_json = nlohmann::json::object();
                    response_json["raw_body"] = response.body;
                    response_json["content_type"] = "text/plain";

                    // Check for common error patterns
                    std::string lower_body = response.body;
                    std::transform(lower_body.begin(), lower_body.end(), lower_body.begin(), ::tolower);

                    if (lower_body.find("redirect") == 0 || lower_body.find("request") == 0) {
                        logger_->warn("Response appears to be a redirect or error message: {}", response.body);
                        response_json["response_type"] = "redirect_or_error";
                    } else if (trimmed_body.find("HTTP/") == 0) {
                        logger_->warn("Received HTTP/1.x response in HTTP/2 connection - possible protocol mismatch");
                        response_json["protocol_error"] = "http1_response_in_http2";
                    } else if (trimmed_body.find("<html") != std::string::npos ||
                              trimmed_body.find("<!DOCTYPE") != std::string::npos) {
                        logger_->warn("Received HTML response instead of JSON - likely server error page");
                        response_json["response_type"] = "html_error_page";
                    } else {
                        logger_->warn("Unknown response format starting with: '{}'",
                                    trimmed_body.length() > 10 ? trimmed_body.substr(0, 10) : trimmed_body);
                        response_json["response_type"] = "unknown_text";
                    }
                }
            }

            // Add metadata to response
            response_json["http_code"] = response.status_code;
            response_json["attempts"] = attempt + 1;

            // Add headers to response
            nlohmann::json headers_json = nlohmann::json::object();
            for (const auto& [key, value] : response.headers) {
                headers_json[key] = value;
            }
            response_json["headers"] = headers_json;            if (!success) {
                if (response.status_code == 0) {
                    logger_->error("Request failed with HTTP code 0 (connection error): {}", response.body);
                } else {
                    logger_->error("Request failed with HTTP code {}: {}",
                                  response.status_code, response.body);
                }
            } else {
                logger_->debug("Request succeeded on attempt {} with HTTP code {}",
                             attempt + 1, response.status_code);
            }

            // Remove response data
            responses_.erase(it);

            return {success, response_json};
        }
        catch (const std::exception& e) {
            logger_->error("Failed to process response on attempt {}: {}", attempt + 1, e.what());
            logger_->error("Raw response body: {}", response.body);
            logger_->error("Response status code: {}", response.status_code);

            // For JSON parse errors with successful HTTP status codes, don't retry
            // The server gave us a response, it's just not the format we expected
            bool is_parse_error_with_success = (response.status_code >= 200 && response.status_code < 300) &&
                                               (std::string(e.what()).find("parse_error") != std::string::npos);

            if (is_parse_error_with_success) {
                logger_->warn("JSON parse error with successful HTTP status - not retrying");

                // Create error response with raw data
                nlohmann::json error_response = nlohmann::json::object();
                error_response["error"] = e.what();
                error_response["raw_body"] = response.body;
                error_response["http_code"] = response.status_code;
                error_response["attempts"] = attempt + 1;

                // Add headers
                nlohmann::json headers_json = nlohmann::json::object();
                for (const auto& [key, value] : response.headers) {
                    headers_json[key] = value;
                }
                error_response["headers"] = headers_json;

                // Remove response data
                responses_.erase(it);

                return {false, error_response};
            }

            // Remove response data
            responses_.erase(it);

            if (attempt == max_retries - 1) {
                return {false, {{"error", e.what()}, {"attempts", max_retries}}};
            }
            continue;
        }
    }

    // Should never reach here
    logger_->error("Unexpected exit from retry loop");
    return {false, {{"error", "unexpected_retry_loop_exit"}, {"attempts", max_retries}}};
}

// Define a struct to hold stream-specific data
struct StreamData {
    std::shared_ptr<std::string> body;
    size_t body_offset = 0; // store how much we've already sent
};

int32_t PcfClientWrapper::submit_request(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& headers,
    const std::string& body) {

    // Prepare nghttp2 headers
    std::vector<nghttp2_nv> nvs;
    nvs.reserve(headers.size());

    for (const auto& [name, value] : headers) {
        nghttp2_nv nv;
        nv.name = (uint8_t*)name.c_str();
        nv.namelen = name.length();
        nv.value = (uint8_t*)value.c_str();
        nv.valuelen = value.length();
        nv.flags = NGHTTP2_NV_FLAG_NONE;
        nvs.push_back(nv);
    }

    // Check if session is initialized
    if (!session_) {
        logger_->error("nghttp2 session is not initialized");
        return -1;
    }

    // Setup data provider for body
    nghttp2_data_provider2 data_provider;
    StreamData* stream_data = nullptr;

    if (!body.empty()) {
        // Non-empty body, set up data provider
        stream_data = new StreamData();
        stream_data->body = std::make_shared<std::string>(body);
        data_provider.source.ptr = stream_data;

        data_provider.read_callback = [](nghttp2_session *session, int32_t stream_id,
                                    uint8_t *buf, size_t length,
                                    uint32_t *data_flags,
                                    nghttp2_data_source *source,
                                    void *user_data) -> ssize_t {

            auto* sd = static_cast<StreamData*>(source->ptr);
            size_t remaining = sd->body->size() - sd->body_offset;
            size_t copylen = std::min(length, remaining);

            if (copylen > 0) {
                memcpy(buf, sd->body->data() + sd->body_offset, copylen);
                sd->body_offset += copylen;
            }

            if (sd->body_offset == sd->body->size()) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            }

            return copylen;
        };
    } else {
        // Empty body
        data_provider.source.ptr = nullptr;
        data_provider.read_callback = [](nghttp2_session *session, int32_t stream_id,
                                    uint8_t *buf, size_t length,
                                    uint32_t *data_flags,
                                    nghttp2_data_source *source,
                                    void *user_data) -> ssize_t {
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            return 0;
        };
    }

    // Submit the request headers
    std::lock_guard<std::mutex> lock(session_mutex_);
    int32_t stream_id = nghttp2_submit_request2(session_, nullptr, nvs.data(),
                                            nvs.size(),
                                            body.empty() ? nullptr : &data_provider,
                                            this);

    if (stream_id <= 0) {
        logger_->error("Failed to submit request headers: {}",
                    nghttp2_strerror((int)stream_id));
        // Clean up stream data if request submission failed
        if (stream_data) {
            delete stream_data;
        }
        return -1;
    }

    // Set stream user data for cleanup tracking
    if (stream_data) {
        nghttp2_session_set_stream_user_data(session_, stream_id, stream_data);
    }

    // Initialize response data structure
    {
        std::lock_guard<std::mutex> lock(responses_mutex_);
        responses_[stream_id] = ResponseData();
    }

    // Send the request
    const uint8_t* data_ptr;
    ssize_t serlen;

    while ((serlen = nghttp2_session_mem_send(session_, &data_ptr)) > 0) {
        try {
            boost::asio::write(socket_, boost::asio::buffer(data_ptr, serlen));
        } catch (const std::exception& e) {
            logger_->error("Failed to send request data: {}", e.what());
            return -1;
        }
    }

    if (serlen < 0) {
        logger_->error("nghttp2_session_mem_send failed: {}",
                    nghttp2_strerror((int)serlen));
        return -1;
    }

    return stream_id;
}

bool PcfClientWrapper::wait_for_response(int32_t stream_id, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        // Check if response is completed
        {
            std::lock_guard<std::mutex> lock(responses_mutex_);
            auto it = responses_.find(stream_id);

            if (it != responses_.end() && it->second.completed) {
                return true;
            }
        }

        // Check timeout
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time).count();

        if (elapsed > timeout_ms) {
            logger_->error("Response timeout for stream {}", stream_id);
            return false;
        }

        logger_->debug("Elapsed time: {} ms, waiting for more data", elapsed);
        // Read more data
        std::vector<uint8_t> buffer(16384);

        try {
            size_t readlen = socket_.read_some(boost::asio::buffer(buffer));

            if (readlen > 0) {
                logger_->debug("Read {} bytes from socket for stream {}", readlen, stream_id);
                process_data(buffer.data(), readlen);
            } else {
                // If read_some returns 0, it means EOF (connection closed by server).
                logger_->debug("Socket read_some returned 0 bytes, likely EOF for stream {}", stream_id);
                connected_ = false; // Mark connection as closed

                // Even if EOF, check if the response was completed just before closure
                std::lock_guard<std::mutex> lock(responses_mutex_);
                auto it = responses_.find(stream_id);
                if (it != responses_.end() && it->second.completed) {
                    logger_->debug("Response size for stream {}: {} bytes",
                              stream_id, it->second.body.size());
                    return true;
                }
                return false; // Not completed and connection closed
            }
        }
        catch (const boost::system::system_error& e) {
            if (e.code() == boost::asio::error::eof) {
                // Connection closed
                logger_->debug("Connection closed by server for stream {}", stream_id);
                connected_ = false;

                // Check if response is completed
                std::lock_guard<std::mutex> lock(responses_mutex_);
                auto it = responses_.find(stream_id);

                if (it != responses_.end() && it->second.completed) {
                    return true;
                }

                return false;
            } else if (e.code() == boost::asio::error::would_block) {
                // No data available, continue waiting
                continue;
            }

            logger_->error("Read error for stream {}: {}", stream_id, e.what());
            connected_ = false;
            return false;
        }
        catch (const std::exception& e) {
            logger_->error("Error waiting for response: {}", e.what());
            return false;
        }

        // Short sleep to avoid tight loop
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    logger_->error("Unexpected exit from wait_for_response loop for stream ID: {}", stream_id);
    return false;
}

ssize_t PcfClientWrapper::process_data(const uint8_t* data, size_t length) {
    if (!session_) {
        logger_->error("nghttp2 session is not initialized");
        return -1;
    }
    try {
        logger_->debug("Processing {} bytes of data", length);
        std::lock_guard<std::mutex> lock(session_mutex_);
        ssize_t processlen = nghttp2_session_mem_recv(session_, data, length);

        if (processlen < 0) {
            logger_->error("Failed to process received data: {}",
                          nghttp2_strerror((int)processlen));
            return -1;
        }

        logger_->debug("Processed {} bytes of data", processlen);

        return processlen;
    } catch (const std::exception& e) {
        logger_->error("Error logging data processing: {}", e.what());
        return -1;
    }
}

int PcfClientWrapper::on_frame_recv_callback(nghttp2_session *session,
                                           const nghttp2_frame *frame,
                                           void *user_data) {
    PcfClientWrapper* client = static_cast<PcfClientWrapper*>(user_data);

    switch (frame->hd.type) {
        case NGHTTP2_HEADERS:
            if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
                client->logger_->debug("Received headers for stream {}",
                                     frame->hd.stream_id);
            }
            break;
        case NGHTTP2_DATA:
            client->logger_->debug("Received data for stream {}",
                                 frame->hd.stream_id);
            break;
        case NGHTTP2_RST_STREAM:
            client->logger_->warn("Stream {} was reset",
                                frame->hd.stream_id);
            break;
    }

    return 0;
}

int PcfClientWrapper::on_header_callback(nghttp2_session *session,
                                       const nghttp2_frame *frame,
                                       const uint8_t *name, size_t namelen,
                                       const uint8_t *value, size_t valuelen,
                                       uint8_t flags, void *user_data) {
    PcfClientWrapper* client = static_cast<PcfClientWrapper*>(user_data);

    if (frame->hd.type == NGHTTP2_HEADERS &&
        frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {

        std::string header_name(reinterpret_cast<const char*>(name), namelen);
        std::string header_value(reinterpret_cast<const char*>(value), valuelen);

        std::lock_guard<std::mutex> lock(client->responses_mutex_);
        auto it = client->responses_.find(frame->hd.stream_id);

        if (it != client->responses_.end()) {
            if (header_name == ":status") {
                it->second.status_code = std::stoi(header_value);
            } else {
                it->second.headers[header_name] = header_value;
            }
        }
    }

    return 0;
}

int PcfClientWrapper::on_data_chunk_recv_callback(nghttp2_session *session,
                                                uint8_t flags, int32_t stream_id,
                                                const uint8_t *data, size_t len,
                                                void *user_data) {
    PcfClientWrapper* client = static_cast<PcfClientWrapper*>(user_data);

    std::lock_guard<std::mutex> lock(client->responses_mutex_);
    auto it = client->responses_.find(stream_id);

    if (it != client->responses_.end()) {
        it->second.body.append(reinterpret_cast<const char*>(data), len);
    }

    return 0;
}

int PcfClientWrapper::on_stream_close_callback(nghttp2_session *session,
                                             int32_t stream_id,
                                             uint32_t error_code,
                                             void *user_data) {
    PcfClientWrapper* client = static_cast<PcfClientWrapper*>(user_data);

    // Clean up stream data first
    void *stream_userdata = nghttp2_session_get_stream_user_data(session, stream_id);
    if (stream_userdata) {
        client->logger_->debug("Cleaning up stream data for stream {}",
                             stream_id);
        auto* sd = static_cast<StreamData*>(stream_userdata);
        delete sd;  // Clean up the allocated stream data
        // Clear the user data pointer to prevent double-free
        nghttp2_session_set_stream_user_data(session, stream_id, nullptr);
        client->logger_->debug("Stream data cleaned up for stream {}",
                             stream_id);
    }

    // Mark response as completed
    {
        std::lock_guard<std::mutex> lock(client->responses_mutex_);
        auto it = client->responses_.find(stream_id);

        if (it != client->responses_.end()) {
            it->second.completed = true;

            if (error_code != 0) {
                client->logger_->warn("Stream {} closed with error: {}",
                                    stream_id, error_code);
                // For critical errors, mark connection as unusable
                if (error_code == 7 || error_code == 2 || error_code == 8) { // PROTOCOL_ERROR, INTERNAL_ERROR, CANCEL
                    client->logger_->warn("Critical error {}, marking connection for reset", error_code);
                    client->connection_error_ = true;
                }
                // For error cases, mark the response with error info
                if (it->second.status_code == 0) {
                    it->second.status_code = 0; // Indicate connection error
                }
            } else {
                client->logger_->debug("Stream {} completed successfully", stream_id);
            }
        }
    }

    client->logger_->debug("Stream {} closed", stream_id);

    return 0;
}

} // namespace southbound
} // namespace af