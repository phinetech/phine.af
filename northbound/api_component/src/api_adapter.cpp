// api_adapter.cpp
#include "api_adapter.h"
#include <iostream>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <boost/asio/signal_set.hpp>
#include "../common/communication/include/communication_factory.h"

namespace af::northbound {

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;

ApiAdapter::ApiAdapter(const std::string& config_path) 
    : AfComponent("api_adapter") {
    initializeLogger(spdlog::level::debug);
    
    logger_->info("Initializing API Adapter");
    
    // Load configuration
    loadConfig(config_path);
    
    // Create API handlers
    api_handlers_ = std::make_shared<ApiHandlers>();
    
    // Create communication interface to AF Core
    std::unordered_map<std::string, std::string> comm_config;
    comm_config["target_service"] = "af_core";
    core_comm_ = af::communication::CommunicationFactory::create_service(
        "grpc", "api_adapter", comm_config);
    logger_->info("Created communication service for AF Core");
    
    try {
        if (!core_comm_->initialize("api_adapter", comm_config)) {
            throw std::runtime_error("Failed to create communication service");
        }
    } catch (const std::exception& e) {
        logger_->error("Error creating communication service: {}", e.what());
        throw;
    }
    // if (!core_comm_->initialize("api_adapter", comm_config)) {
    //     logger_->error("Failed to initialize communication with AF Core");
    // }
}

ApiAdapter::~ApiAdapter() {
    stop();
}

void ApiAdapter::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("api_adapter");
    
    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("api_adapter");
    }
    
    // Set the log level
    logger_->set_level(log_level);
    
    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void ApiAdapter::initialize() {
    logger_->info("Initializing HTTP/2 API server on {}:{}", host_, port_);
    
    try {
        // Create io_context with thread pool
        io_context_ = std::make_shared<boost::asio::io_context>(threads_);
        boost::system::error_code ec;
        
        // Initialize API handlers with communication interface
        api_handlers_->initialize(core_comm_);

        // Start the communication service
        if (!core_comm_->start()) {
            logger_->error("Failed to start communication service");
        }

        server_ = std::make_unique<http2>();

        // Setup routes
        setupRoutes();

        // Create HTTP/2 server
        if (tls_enabled_) {
            logger_->info("TLS enabled for HTTP/2 server");
            boost::system::error_code ec;
            
            if (!cert_file_.empty() && !key_file_.empty()) {
                boost::asio::ssl::context tls_context(boost::asio::ssl::context::sslv23);
                tls_context.use_certificate_chain_file(cert_file_);
                tls_context.use_private_key_file(key_file_, boost::asio::ssl::context::pem);
                
                
                if (server_->listen_and_serve(ec, tls_context, host_, std::to_string(port_))) {
                    logger_->error("Failed to start HTTP/2 server: {}", ec.message());
                    throw std::runtime_error("Failed to start HTTP/2 server");
                }
            } else {
                logger_->warn("TLS enabled but certificate or key file not provided. Falling back to non-TLS.");
                if (server_->listen_and_serve(ec, host_, std::to_string(port_))) {
                    logger_->error("Failed to start HTTP/2 server: {}", ec.message());
                    throw std::runtime_error("Failed to start HTTP/2 server");
                }
            }
        } else {
            logger_->info("TLS not enabled for HTTP/2 server");
            if (server_->listen_and_serve(ec, host_, std::to_string(port_), false)) {
                logger_->error("Failed to start HTTP/2 server: {}", ec.message());
                throw std::runtime_error("Failed to start HTTP/2 server");
            }
        }
        
        logger_->info("HTTP/2 API server initialized");
    } catch (const std::exception& e) {
        logger_->error("Failed to initialize API handlers: {}", e.what());
        throw;
    }
}

void ApiAdapter::setupRoutes() {
    logger_->info("Setting up HTTP/2 API server routes");
    // Health endpoint
    server_->handle("/health", [this](const request &req, const response &res) {
        logger_->info("Received health check request");
        api_handlers_->getHealth(req, res);
    });
    
    // Version endpoint
    server_->handle("/version", [this](const request &req, const response &res) {
        logger_->info("Received version request");
        api_handlers_->getVersion(req, res);
    });
    
    // QoS endpoint
    server_->handle("/qos", [this](const request &req, const response &res) {
        if (req.method() == "POST") {
            api_handlers_->requestQoS(req, res);
        } else {
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(405, headers);
            res.end("{\"error\":\"method_not_allowed\",\"message\":\"Method not allowed\"}");
        }
    });
    
    // Subscriptions endpoints
    server_->handle("/subscriptions", [this](const request &req, const response &res) {
        if (req.method() == "GET") {
            api_handlers_->getSubscriptions(req, res);
        } else if (req.method() == "POST") {
            api_handlers_->createSubscription(req, res);
        } else {
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(405, headers);
            res.end("{\"error\":\"method_not_allowed\",\"message\":\"Method not allowed\"}");
        }
    });
    
    // Single subscription endpoints
    server_->handle("/subscriptions/", [this](const request &req, const response &res) {
        std::string path = req.uri().path;
        // Extract ID from path (after /subscriptions/)
        std::string id = path.substr(std::string("/subscriptions/").length());
        
        if (id.empty()) {
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(400, headers);
            res.end("{\"error\":\"bad_request\",\"message\":\"Missing subscription ID\"}");
            return;
        }
        
        if (req.method() == "GET") {
            api_handlers_->getSubscription(req, res, id);
        } else if (req.method() == "DELETE") {
            api_handlers_->deleteSubscription(req, res, id);
        } else {
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(405, headers);
            res.end("{\"error\":\"method_not_allowed\",\"message\":\"Method not allowed\"}");
        }
    });

    // Set default handler for unmatched routes
    server_->handle("/", [this](const request &req, const response &res) {
        // This lambda will be called if no other more specific path handler (like /version, /health)
        // has handled the request.
        
        logger_->warn("Unhandled request path: {}. Responding with 404 Not Found.", req.uri().path);

        header_map headers;
        headers.emplace("content-type", header_value{{"application/json"}, false});
        res.write_head(404, headers); // HTTP 404 Not Found
        res.end("{\"error\":\"not_found\",\"message\":\"The requested resource or endpoint was not found on this server.\"}");
    });

    logger_->info("HTTP/2 API server routes set up successfully");
}

void ApiAdapter::start() {
    try {
        if (!server_) {
            logger_->error("Cannot start HTTP/2 API server: not initialized");
            return;
        }
        
        logger_->info("Starting HTTP/2 API server");
        
        // Create a thread pool to run the io_context
        std::vector<std::thread> threads;
        for (int i = 0; i < threads_; ++i) {
            threads.emplace_back([this]() {
                io_context_->run();
            });
        }
        
        // Wait for threads to complete (which they won't unless stop() is called)
        for (auto &t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    } catch (const std::exception& e) {
        logger_->error("Failed to initialize API Adapter: {}", e.what());
        return;
    }
}

void ApiAdapter::stop() {
    if (io_context_) {
        logger_->info("Stopping HTTP/2 API server");
        io_context_->stop();
        server_.reset();
    }
    
    // Stop the communication service
    if (core_comm_) {
        core_comm_->stop();
    }
}

void ApiAdapter::configure(const std::map<std::string, std::string>& settings) {
    for (const auto& [key, value] : settings) {
        if (key == "host") {
            host_ = value;
        } else if (key == "port") {
            port_ = std::stoi(value);
        } else if (key == "threads") {
            threads_ = std::stoi(value);
        } else if (key == "tls_enabled") {
            tls_enabled_ = (value == "true" || value == "1");
        } else if (key == "cert_file") {
            cert_file_ = value;
        } else if (key == "key_file") {
            key_file_ = value;
        }
    }
    
    logger_->info("HTTP/2 API server reconfigured: {}:{} with {} threads, TLS: {}", 
                 host_, port_, threads_, tls_enabled_ ? "enabled" : "disabled");
}

void ApiAdapter::loadConfig(const std::string& config_path) {
    try {
        YAML::Node config = YAML::LoadFile(config_path);
        
        std::cout << "Loading configuration from: " << config_path << std::endl;
        host_ = config["api_adapter"]["host"].as<std::string>("0.0.0.0");
        port_ = config["api_adapter"]["port"].as<int>(8080);
        threads_ = config["api_adapter"]["threads"].as<int>(4);
        tls_enabled_ = config["api_adapter"]["tls_enabled"].as<bool>(false);
        cert_file_ = config["api_adapter"]["cert_file"].as<std::string>("");
        key_file_ = config["api_adapter"]["key_file"].as<std::string>("");
        
        std::cout << "Configuration loaded successfully" << std::endl;
        logger_->info("Loaded configuration: host={}, port={}, threads={}, tls={}", 
                     host_, port_, threads_, tls_enabled_ ? "enabled" : "disabled");
    }
    catch (const std::exception& e) {
        logger_->error("Failed to load configuration: {}", e.what());
        // Set default values
        host_ = "0.0.0.0";
        port_ = 8080;
        threads_ = 4;
        tls_enabled_ = false;
        cert_file_ = "";
        key_file_ = "";
    }
}

} // namespace af::northbound