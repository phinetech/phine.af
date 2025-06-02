// api_adapter.h
#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <nghttp2/asio_http2_server.h>
#include "api_handlers.h"
#include "../../common/component/include/af_component.h"
#include "../../common/communication/include/communication_interface.h"

namespace af::northbound {

class ApiAdapter : public af::common::AfComponent {
public:
    ApiAdapter(const std::string& config_path);
    ~ApiAdapter();

    // Initialize the API server
    void initialize() override;

    // Start the API server
    void start() override;

    // Stop the API server
    void stop() override;

    // Configure the API server with runtime settings
    void configure(const std::map<std::string, std::string>& settings);

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Server configuration
    std::string host_;
    int port_;
    int threads_;
    bool tls_enabled_;
    std::string cert_file_;
    std::string key_file_;
    
    // HTTP/2 server and io_context
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<nghttp2::asio_http2::server::http2> server_;
    
    // API handlers
    std::shared_ptr<ApiHandlers> api_handlers_;
    
    // Communication interface for talking to AF Core
    std::shared_ptr<af::communication::CommunicationService> core_comm_;
    
    // Logger instance
    std::shared_ptr<spdlog::logger> logger_;
    
    // Load configuration
    void loadConfig(const std::string& config_path);
    
    // Set up HTTP/2 server routes
    void setupRoutes();
};

} // namespace af::northbound