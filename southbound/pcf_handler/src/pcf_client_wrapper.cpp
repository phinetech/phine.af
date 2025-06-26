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
      session_(nullptr), socket_(io_context_), connected_(false) {
    
    // Setup logger
    initializeLogger(spdlog::level::debug);
    
    logger_->info("PCF Client Wrapper created");
    
    // Parse base URL
    parse_url(base_url_);
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
    if (connected_) {
        return true;
    }
    
    logger_->debug("Connecting to {}:{} with HTTP/2 prior knowledge", host_, port_);
    
    try {
        // Resolve the host
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(host_, port_);
        
        // Connect to the host
        boost::asio::connect(socket_, endpoints);
        
        // Configure proper HTTP/2 settings
        nghttp2_settings_entry iv[] = {
            {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
            {NGHTTP2_SETTINGS_ENABLE_PUSH, 0},            // Disable server push
            {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 65535} // Default window size
        };
        
        nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv, 
                               sizeof(iv) / sizeof(iv[0]));
        
        // Send the SETTINGS frame
        std::vector<uint8_t> buffer(16384);
        const uint8_t* data_ptr;
        ssize_t serlen = nghttp2_session_mem_send(session_, &data_ptr);
        
        if (serlen > 0) {
            boost::asio::write(socket_, boost::asio::buffer(data_ptr, serlen));
        }
        
        // Exchange frames to complete the handshake
        bool handshake_complete = false;
        int attempt = 0;
        
        while (!handshake_complete && attempt < 5) {
            attempt++;
            
            try {
                // Read from socket
                buffer.resize(16384);
                size_t readlen = socket_.read_some(boost::asio::buffer(buffer));
                
                if (readlen > 0) {
                    
                    // Process the data
                    ssize_t processlen = nghttp2_session_mem_recv(session_, buffer.data(), readlen);
                    
                    if (processlen < 0) {
                        logger_->error("Error processing received data: {}", 
                                       nghttp2_strerror((int)processlen));
                        return false;
                    }
                    
                    // Debug the received frames
                    logger_->debug("Processed {} bytes of HTTP/2 frames", processlen);
                }
                
                // Send any pending data
                while ((serlen = nghttp2_session_mem_send(session_, &data_ptr)) > 0) {
                    boost::asio::write(socket_, boost::asio::buffer(data_ptr, serlen));
                }
                
                // If we've successfully exchanged SETTINGS frames, consider handshake complete
                if (attempt >= 2) {
                    handshake_complete = true;
                }
            }
            catch (const boost::system::system_error& e) {
                if (e.code() == boost::asio::error::eof) {
                    logger_->error("Server closed connection during handshake");
                    return false;
                }
                throw;
            }
        }
        
        connected_ = true;
        logger_->info("HTTP/2 connection established successfully");
        
        // Initialize response map for future requests
        responses_.clear();
        
        return true;
    }
    catch (const std::exception& e) {
        logger_->error("Connection failed: {}", e.what());
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

bool PcfClientWrapper::delete_app_session(const std::string& app_session_id) {
    logger_->info("Deleting application session: {}", app_session_id);
    
    // Construct path
    std::string path = "app-sessions/" + app_session_id;
    
    // Perform DELETE request
    auto result = perform_request("DELETE", path);
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
    
    // Ensure we're connected
    if (!connected_ && !connect()) {
        return {false, {{"error", "connection_failed"}}};
    }
    
    // Set headers
    std::map<std::string, std::string> headers = {
        {":method", method},
        {":scheme", use_tls_ ? "https" : "http"},
        {":authority", host_ + ":" + port_},
        {":path", full_path},
        {"content-type", "application/json"},
        {"accept", "application/json"}
    };
    
    // Prepare request body
    std::string request_body;
    if (!request_data.empty()) {
        request_body = request_data.dump();
        headers["content-length"] = std::to_string(request_body.length());
    }

    logger_->debug("Request body: {}", request_body);
    
    // Submit the request
    int32_t stream_id = submit_request(method, full_path, headers, request_body);
    
    if (stream_id < 0) {
        logger_->error("Failed to submit request");
        return {false, {{"error", "request_submission_failed"}}};
    }

    // Wait for response
    if (!wait_for_response(stream_id)) {
        logger_->error("Request timed out");
        return {false, {{"error", "request_timeout"}}};
    }

    // Get response
    std::lock_guard<std::mutex> lock(responses_mutex_);
    auto it = responses_.find(stream_id);
    
    if (it == responses_.end() || !it->second.completed) {
        logger_->error("No response received for stream {}", stream_id);
        return {false, {{"error", "no_response"}}};
    }
    
    ResponseData& response = it->second;
    
    // Check HTTP status code
    bool success = (response.status_code >= 200 && response.status_code < 300);
    
    try {
        // Parse response body if available
        nlohmann::json response_json;
        
        if (!response.body.empty()) {
            response_json = nlohmann::json::parse(response.body);
        }
        
        // Add status code to response
        response_json["http_code"] = response.status_code;
        
        if (!success) {
            logger_->error("Request failed with HTTP code {}: {}", 
                          response.status_code, response.body);
        }
        
        // Remove response data
        responses_.erase(it);
        
        return {success, response_json};
    }
    catch (const std::exception& e) {
        logger_->error("Failed to parse response: {}", e.what());
        
        // Remove response data
        responses_.erase(it);
        
        return {false, {{"error", e.what()}}};
    }
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
    
    if (!body.empty()) {
        // Non-empty body, set up data provider
        auto stream_data = new StreamData();
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
        return -1;
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
                process_data(buffer.data(), readlen);
            }

            return true; // Data read successfully
        }
        catch (const boost::system::system_error& e) {
            if (e.code() == boost::asio::error::eof) {
                // Connection closed
                logger_->debug("Connection closed by server");
                connected_ = false;
                
                // Check if response is completed
                std::lock_guard<std::mutex> lock(responses_mutex_);
                auto it = responses_.find(stream_id);
                
                if (it != responses_.end() && it->second.completed) {
                    return true;
                }
                
                return false;
            }
            
            logger_->error("Read error: {}", e.what());
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
    
    std::lock_guard<std::mutex> lock(client->responses_mutex_);
    void *stream_userdata = nghttp2_session_get_stream_user_data(session, stream_id);
    if (stream_userdata) {
        client->logger_->debug("Cleaning up stream data for stream {}", 
                             stream_id);
        auto* sd = static_cast<StreamData*>(stream_userdata);
        delete sd;  // cleanup here
    }

    auto it = client->responses_.find(stream_id);
    
    if (it != client->responses_.end()) {
        it->second.completed = true;
        
        if (error_code != 0) {
            client->logger_->warn("Stream {} closed with error: {}", 
                                stream_id, error_code);
        } else {
            client->logger_->debug("Stream {} completed successfully", stream_id);
        }
    }

    client->logger_->debug("Stream {} closed", stream_id);
    
    return 0;
}

} // namespace southbound
} // namespace af