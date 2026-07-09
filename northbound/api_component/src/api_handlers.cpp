// api_handlers.cpp
#include "api_handlers.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <message.h>

namespace af::northbound {

using json = nlohmann::json;
using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;

ApiHandlers::ApiHandlers() {
    // Setup logger
    initializeLogger(spdlog::level::debug);

    logger_->info("API Handlers initialized");
}

ApiHandlers::~ApiHandlers() {
}

void ApiHandlers::initialize(std::shared_ptr<af::communication::CommunicationService> core_comm,
                             const std::string& core_destination) {
    core_comm_ = core_comm;
    core_destination_ = core_destination;
    logger_->info("API Handlers initialized with core communication interface to {}",
                  core_destination_);
}

void ApiHandlers::getHealth(const request& req, const response& res) {
    json j = {
        {"status", "up"},
        {"timestamp", std::time(nullptr)}
    };

    header_map headers;
    headers.emplace("content-type", header_value{{"application/json"}, false});
    res.write_head(200, headers);
    res.end(j.dump());
}

void ApiHandlers::getVersion(const request& req, const response& res) {
    json j = {
        {"version", "1.0.0"},
        {"api_version", "v1"},
        {"build_date", __DATE__},
        {"build_time", __TIME__}
    };

    header_map headers;
    headers.emplace("content-type", header_value{{"application/json"}, false});
    res.write_head(200, headers);
    res.end(j.dump());
}

void ApiHandlers::requestQoS(const request& req, const response& res) {
    readRequestBody(req, [this, &res](const std::string& body) {
        try {
            // Parse request body
            auto json_body = json::parse(body);

            // Log the received request
            logger_->info("Received QoS request: {}", json_body.dump());

            // Create message to AF Core
            auto msg = createMessage("qos_request", json_body.dump());

            // Send to AF Core and wait for response
            auto reply = core_comm_->send_request(core_destination_, msg);

            // Send response back to client
            if (reply && reply->message_type == "qos_success") {
                auto reply_body = json::parse(reply->payload);
                header_map headers;
                headers.emplace("content-type", header_value{{"application/json"}, false});
                res.write_head(200, headers);
                res.end(reply_body.dump());
            } else if (reply && reply->message_type == "qos_error") {
                auto error_body = json::parse(reply->payload);
                header_map headers;
                headers.emplace("content-type", header_value{{"application/json"}, false});
                res.write_head(400, headers);
                res.end(error_body.dump());
            } else {
                // Unexpected response or no response
                json error = {
                    {"error", "unexpected_response"},
                    {"message", "Received unexpected response from core service"}
                };
                header_map headers;
                headers.emplace("content-type", header_value{{"application/json"}, false});
                res.write_head(500, headers);
                res.end(error.dump());
            }
        } catch (const std::exception& e) {
            // Error handling
            logger_->error("Error processing QoS request: {}", e.what());
            json error = {
                {"error", "bad_request"},
                {"message", e.what()}
            };
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(400, headers);
            res.end(error.dump());
        }
    });
}

void ApiHandlers::getSubscriptions(const request& req, const response& res) {
    try {
        // Create message to AF Core
        auto msg = createMessage("get_subscriptions", "");

        // Send to AF Core and wait for response
        auto reply = core_comm_->send_request(core_destination_, msg);

        // Send response back to client
        if (reply && reply->message_type == "subscriptions_list") {
            auto subscriptions = json::parse(reply->payload);
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(200, headers);
            res.end(subscriptions.dump());
        } else {
            // Error response
            json error = {
                {"error", "internal_error"},
                {"message", "Failed to retrieve subscriptions"}
            };
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(500, headers);
            res.end(error.dump());
        }
    } catch (const std::exception& e) {
        // Error handling
        logger_->error("Error retrieving subscriptions: {}", e.what());
        json error = {
            {"error", "internal_error"},
            {"message", e.what()}
        };
        header_map headers;
        headers.emplace("content-type", header_value{{"application/json"}, false});
        res.write_head(500, headers);
        res.end(error.dump());
    }
}

void ApiHandlers::createSubscription(const request& req, const response& res) {
    readRequestBody(req, [this, &res](const std::string& body) {
        try {
            // Parse request body
            auto json_body = json::parse(body);

            // Log the received request
            logger_->info("Received subscription creation request: {}", json_body.dump());

            // Create message to AF Core
            auto msg = createMessage("create_subscription", json_body.dump());

            // Send to AF Core and wait for response
            auto reply = core_comm_->send_request(core_destination_, msg);

            // Send response back to client
            if (reply && reply->message_type == "subscription_created") {
                auto subscription = json::parse(reply->payload);
                header_map headers;
                headers.emplace("content-type", header_value{{"application/json"}, false});
                res.write_head(201, headers);
                res.end(subscription.dump());
            } else if (reply && reply->message_type == "subscription_error") {
                auto error_body = json::parse(reply->payload);
                header_map headers;
                headers.emplace("content-type", header_value{{"application/json"}, false});
                res.write_head(400, headers);
                res.end(error_body.dump());
            } else {
                // Unexpected response
                json error = {
                    {"error", "unexpected_response"},
                    {"message", "Received unexpected response from core service"}
                };
                header_map headers;
                headers.emplace("content-type", header_value{{"application/json"}, false});
                res.write_head(500, headers);
                res.end(error.dump());
            }
        } catch (const std::exception& e) {
            // Error handling
            logger_->error("Error creating subscription: {}", e.what());
            json error = {
                {"error", "bad_request"},
                {"message", e.what()}
            };
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(400, headers);
            res.end(error.dump());
        }
    });
}

void ApiHandlers::getSubscription(const request& req, const response& res, const std::string& id) {
    try {
        // Create message to AF Core
        json content = {{"id", id}};
        auto msg = createMessage("get_subscription", content.dump());

        // Send to AF Core and wait for response
        auto reply = core_comm_->send_request(core_destination_, msg);

        // Send response back to client
        if (reply && reply->message_type == "subscription") {
            auto subscription = json::parse(reply->payload);
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(200, headers);
            res.end(subscription.dump());
        } else if (reply && reply->message_type == "subscription_not_found") {
            json error = {
                {"error", "not_found"},
                {"message", "Subscription not found"}
            };
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(404, headers);
            res.end(error.dump());
        } else {
            // Unexpected response
            json error = {
                {"error", "unexpected_response"},
                {"message", "Received unexpected response from core service"}
            };
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(500, headers);
            res.end(error.dump());
        }
    } catch (const std::exception& e) {
        // Error handling
        logger_->error("Error retrieving subscription: {}", e.what());
        json error = {
            {"error", "internal_error"},
            {"message", e.what()}
        };
        header_map headers;
        headers.emplace("content-type", header_value{{"application/json"}, false});
        res.write_head(500, headers);
        res.end(error.dump());
    }
}

void ApiHandlers::deleteSubscription(const request& req, const response& res, const std::string& id) {
    try {
        // Create message to AF Core
        json content = {{"id", id}};
        auto msg = createMessage("delete_subscription", content.dump());

        // Send to AF Core and wait for response
        auto reply = core_comm_->send_request(core_destination_, msg);

        // Send response back to client
        if (reply && reply->message_type == "subscription_deleted") {
            res.write_head(204);
            res.end();
        } else if (reply && reply->message_type == "subscription_not_found") {
            json error = {
                {"error", "not_found"},
                {"message", "Subscription not found"}
            };
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(404, headers);
            res.end(error.dump());
        } else {
            // Unexpected response
            json error = {
                {"error", "unexpected_response"},
                {"message", "Received unexpected response from core service"}
            };
            header_map headers;
            headers.emplace("content-type", header_value{{"application/json"}, false});
            res.write_head(500, headers);
            res.end(error.dump());
        }
    } catch (const std::exception& e) {
        // Error handling
        logger_->error("Error deleting subscription: {}", e.what());
        json error = {
            {"error", "internal_error"},
            {"message", e.what()}
        };
        header_map headers;
        headers.emplace("content-type", header_value{{"application/json"}, false});
        res.write_head(500, headers);
        res.end(error.dump());
    }
}

void ApiHandlers::readRequestBody(const request& req, std::function<void(const std::string&)> callback) {
    std::string body;

    req.on_data([&body, callback](const uint8_t* data, size_t len) {
        if (len > 0) {
            body.append(reinterpret_cast<const char*>(data), len);
        } else {
            // End of data, call the callback with the complete body
            callback(body);
        }
        return true;  // Continue reading
    });
}

// Helper to generate a correlation ID
std::string ApiHandlers::generateCorrelationId() {
    static std::atomic<uint64_t> counter(0);
    std::stringstream ss;
    ss << "api-" << std::time(nullptr) << "-" << counter++;
    return ss.str();
}

af::communication::MessagePtr ApiHandlers::createMessage(const std::string& type, const std::string& content) {
    auto msg = std::make_shared<af::communication::Message>();
    msg->message_type = type;
    msg->payload.assign(content.begin(), content.end());
    // TODO: Chekc if we need to set source, and update it in the message model
    // msg->source = "api_adapter";
    msg->correlation_id = generateCorrelationId();
    return msg;
}

void ApiHandlers::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("northbound::api_handlers");

    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("northboun::api_handlers");
    }

    // Set the log level
    logger_->set_level(log_level);

    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

} // namespace af::northbound