// api_handlers.h
#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <sstream>
#include <functional>
#include <nghttp2/asio_http2_server.h>
#include <spdlog/spdlog.h>
#include <communication_interface.h>

namespace af::northbound {

class ApiHandlers {
public:
    ApiHandlers();
    ~ApiHandlers();

    // Initialize handlers with communication interface to AF Core
    void initialize(std::shared_ptr<af::communication::CommunicationService> core_comm,
                    const std::string& core_destination);

    // API endpoint handlers
    void getHealth(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res);
    void getVersion(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res);
    void requestQoS(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res);
    void getSubscriptions(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res);
    void createSubscription(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res);
    void getSubscription(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res, const std::string& id);
    void deleteSubscription(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res, const std::string& id);

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Communication interface for talking to AF Core
    std::shared_ptr<af::communication::CommunicationService> core_comm_;
    std::string core_destination_;

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

    // Helper method to read the request body
    void readRequestBody(const nghttp2::asio_http2::server::request& req,
                         std::function<void(const std::string&)> callback);

    // Helper method to create a message
    af::communication::MessagePtr createMessage(const std::string& type, const std::string& content);

    // Helper method to generate correlation ID
    std::string generateCorrelationId();
};

} // namespace af::northbound