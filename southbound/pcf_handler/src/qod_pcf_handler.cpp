/**
 * @file qod_pcf_handler.cpp
 * @brief Implementation of the QoD PCF adapter
 */

#include "qod_pcf_handler.h"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <random>
#include <sstream>
#include <iomanip>

#include "helpers.h"
#include "handlers/service_handler_helpers.h"
#include "models/qod/qos_profile_mapper.h"

namespace af {
namespace southbound {

QodPcfHandler::QodPcfHandler(const std::string& config_path)
    : config_path_(config_path) {

    // Setup logger
    initializeLogger(spdlog::level::debug);

    // Load configuration
    load_config(config_path_);

    pcf_client_ = std::make_shared<PcfClientWrapper>(
        pcf_base_url_, use_tls_, api_version_);

    initialize();

    logger_->info("QoD PCF Adapter created");
}

QodPcfHandler::~QodPcfHandler() {
    // Clean up resources
}

void QodPcfHandler::initialize() {
    logger_->info("QoD PCF Adapter initialized");

    // Initialize PCF client
    pcf_client_->initialize();

    // // Initialize PCC rule manager
    // pcc_rule_manager_->initialize();

    // // Initialize communication with AF Core
    // initialize_communication();

    // // Register message handlers
    // register_handlers();

    logger_->info("PCF Handler initialization complete");
}

void QodPcfHandler::load_config(const std::string& path) {
    try {
        logger_->info("Loading configuration from {}", config_path_);
        YAML::Node config = YAML::LoadFile(config_path_);

        // Load PCF connection details
        pcf_base_url_ = config["pcf_handler"]["pcf_base_url"].as<std::string>(
            "http://pcf:80/npcf-policyauthorization/v1");

        use_tls_ = config["pcf_handler"]["use_tls"].as<bool>(false);
        api_version_ = config["pcf_handler"]["api_version"].as<std::string>("v1");

        logger_->info("PCF base URL: {}", pcf_base_url_);
        logger_->info("Using TLS: {}", use_tls_ ? "true" : "false");
        logger_->info("API version: {}", api_version_);
    }
    catch (const std::exception& e) {
        logger_->error("Failed to load configuration: {}", e.what());

        // Set default values
        pcf_base_url_ = "http://pcf:80/npcf-policyauthorization/v1";
        use_tls_ = false;
        api_version_ = "v1";
    }
}

// Register handlers with communication service
// TODO: Fix me
// void QodPcfHandler::register_handlers() {
//     auto comm_services = orchestrator_->get_communication_services();
//     auto pcf_comm_it = comm_services.find("pcf");

//     // Register PCF response handlers for QoD
//     request_router_->register_handler("pcf_qod_session_created",
//         [this](const af::communication::MessagePtr& msg) {
//             auto [success, pcf_session_id] = qod_pcf_adapter_->handle_pcf_create_response(msg);

//             // Notify session manager of PCF response
//             qod_session_manager_->handle_pcf_session_response(
//                 msg->correlation_id, pcf_session_id, success,
//                 success ? "" : "PCF session creation failed");

//             // Return empty response (already handled internally)
//             return std::make_shared<af::communication::Message>();
//         });

//     request_router_->register_handler("pcf_qod_session_updated",
//         [this](const af::communication::MessagePtr& msg) {
//             bool success = qod_pcf_adapter_->handle_pcf_update_response(msg);

//             // Log the result
//             if (success) {
//                 logger_->info("PCF session updated successfully");
//             } else {
//                 logger_->error("PCF session update failed");
//             }

//             return std::make_shared<af::communication::Message>();
//         });

//     request_router_->register_handler("pcf_qod_session_deleted",
//         [this](const af::communication::MessagePtr& msg) {
//             bool success = qod_pcf_adapter_->handle_pcf_delete_response(msg);

//             if (success) {
//                 logger_->info("PCF session deleted successfully");
//             } else {
//                 logger_->error("PCF session deletion failed");
//             }

//             return std::make_shared<af::communication::Message>();
//         });

//     request_router_->register_handler("pcf_qod_notification",
//         [this](const af::communication::MessagePtr& msg) {
//             auto [qod_session_id, reason] = qod_pcf_adapter_->handle_pcf_notification(msg);

//             if (qod_session_id) {
//                 // Get PCF session ID from adapter
//                 auto pcf_info = qod_pcf_adapter_->get_pcf_session_info(*qod_session_id);
//                 if (pcf_info) {
//                     qod_session_manager_->handle_pcf_session_terminated(
//                         pcf_info->pcf_session_id,
//                         reason.value_or("NETWORK_TERMINATED"));
//                 }
//             }

//             return std::make_shared<af::communication::Message>();
//         });

//     // Register QoD notification callback handler (for CloudEvents delivery results)
//     request_router_->register_handler("qod_notification_result",
//         [this](const af::communication::MessagePtr& msg) {
//             // Handle notification delivery results
//             try {
//                 std::string payload_str(msg->payload.begin(), msg->payload.end());
//                 auto result_json = nlohmann::json::parse(payload_str);

//                 bool success = result_json.value("success", false);
//                 std::string session_id = result_json.value("session_id", "");
//                 std::string error = result_json.value("error", "");

//                 if (!success) {
//                     logger_->warn("QoD notification delivery failed for session {}: {}",
//                                  session_id, error);
//                 }
//             } catch (const std::exception& e) {
//                 logger_->error("Error processing notification result: {}", e.what());
//             }

//             return std::make_shared<af::communication::Message>();
//         });
// }

void QodPcfHandler::initializeLogger(spdlog::level::level_enum log_level) {
    logger_ = spdlog::get("qod_pcf_adapter");

    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("qod_pcf_adapter");
    }

    logger_->set_level(log_level);
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

// === Qod Handlers ===
af::communication::MessagePtr QodPcfHandler::handle_qod_create_pcf_session(
    const af::communication::MessagePtr& message) {

    logger_->info("Handling QoD create PCF session request");

    // Parse incoming message to QodSession
    af::common::qod::QodSession qod_session;
    try {
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto json = nlohmann::json::parse(payload_str);

        // Extract fields
        qod_session.session_id = json.at("session_id").get<std::string>();
        qod_session.device = parse_device(json.at("device"));
        qod_session.application_server = parse_application_server(json.at("application_server"));
        qod_session.device_ports = parse_ports(json.value("device_ports", nlohmann::json{}));
        qod_session.application_server_ports = parse_ports(json.value("application_server_ports", nlohmann::json{}));
        qod_session.qos_profile = json.at("qos_profile").get<std::string>();
        qod_session.duration = std::chrono::seconds(json.at("duration").get<int>());
        if (json.contains("sink")) {
            qod_session.sink = json.at("sink").get<std::string>();
        }

        // Optional fields
        if (json.contains("ue_supi")) {
            qod_session.ue_supi = Supi{json.at("ue_supi").get<std::string>()};
        }

        // CRITICAL: Parse QoS profile mapping (bandwidth values, 5QI, etc.)
        if (json.contains("qos_profile_mapping") && !json["qos_profile_mapping"].is_null()) {
            af::common::qod::QosProfileMapping mapping;
            auto parse_result = af::common::qod::QosProfileMapper::try_deserialize_qos_mapping(
                json["qos_profile_mapping"],
                mapping);
            auto validation_result = af::common::qod::QosProfileMapper::validate_qos_mapping(mapping);

            if (!parse_result.is_valid || !validation_result.is_valid) {
                // Combine validation errors using helper
                std::ostringstream error_stream;
                error_stream << "Invalid qos_profile_mapping";
                af::utils::append_validation_errors(error_stream, parse_result);
                af::utils::append_validation_errors(error_stream, validation_result);

                return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                    400,
                    "bad_request",
                    error_stream.str(),
                    message->correlation_id,
                    logger_);
            }

            qod_session.qos_profile_mapping = mapping;
            logger_->debug("Parsed QoS profile mapping - 5QI: {}, GBR: {}, GBR_DL: {}, MBR_DL: {}",
                          mapping.fiveqi, mapping.is_gbr,
                          mapping.guaranteed_downlink_rate.value_or("N/A"),
                          mapping.max_downlink_rate.value_or("N/A"));
        } else {
            logger_->warn("QoS profile mapping not found in message for session: {}", qod_session.session_id);
        }

        logger_->debug("Parsed QoD session: {}", json.dump());
        // Create PCF session
        auto pcf_msg_opt = create_pcf_session(qod_session);
        if (!pcf_msg_opt) {
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                500,
                "pcf_request_creation_failed",
                "Failed to create PCF session request",
                message->correlation_id,
                logger_);
        }
        return *pcf_msg_opt;

    } catch (const std::exception& e) {
        logger_->error("Error parsing QoD create request: {}", e.what());

        // Return error response
        return af::common::handlers::ServiceHandlerHelpers::create_error_response(
            400,
            "bad_request",
            e.what(),
            message->correlation_id,
            logger_);
    }
}

af::communication::MessagePtr QodPcfHandler::handle_qod_update_pcf_session(
    const af::communication::MessagePtr& message) {

    logger_->info("Handling QoD update PCF session request");

    // Parse incoming message to QodSession
    af::common::qod::QodSession qod_session;
    try {
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto json = nlohmann::json::parse(payload_str);

        // Extract fields
        qod_session.session_id = json.at("session_id").get<std::string>();
        qod_session.api_consumer_id = json.at("api_consumer_id").get<std::string>();
        qod_session.device = parse_device(json.at("device"));
        qod_session.application_server = parse_application_server(json.at("application_server"));
        qod_session.device_ports = parse_ports(json.value("device_ports", nlohmann::json{}));
        qod_session.application_server_ports = parse_ports(json.value("application_server_ports", nlohmann::json{}));
        qod_session.qos_profile = json.at("qos_profile").get<std::string>();
        qod_session.duration = std::chrono::seconds(json.at("duration").get<int>());
        if (json.contains("sink")) {
            qod_session.sink = json.at("sink").get<std::string>();
        }

        // Optional fields
        if (json.contains("ue_supi")) {
            qod_session.ue_supi = Supi{json.at("ue_supi").get<std::string>()};
        }

        // CRITICAL: Parse QoS profile mapping (bandwidth values, 5QI, etc.)
        if (json.contains("qos_profile_mapping") && !json["qos_profile_mapping"].is_null()) {
            af::common::qod::QosProfileMapping mapping;
            auto parse_result = af::common::qod::QosProfileMapper::try_deserialize_qos_mapping(
                json["qos_profile_mapping"],
                mapping);
            auto validation_result = af::common::qod::QosProfileMapper::validate_qos_mapping(mapping);

            if (!parse_result.is_valid || !validation_result.is_valid) {
                // Combine validation errors using helper
                std::ostringstream error_stream;
                error_stream << "Invalid qos_profile_mapping";
                af::utils::append_validation_errors(error_stream, parse_result);
                af::utils::append_validation_errors(error_stream, validation_result);

                return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                    400,
                    "bad_request",
                    error_stream.str(),
                    message->correlation_id,
                    logger_);
            }

            qod_session.qos_profile_mapping = mapping;
            logger_->debug("Parsed QoS profile mapping for update - 5QI: {}, GBR: {}, GBR_DL: {}, MBR_DL: {}",
                          mapping.fiveqi, mapping.is_gbr,
                          mapping.guaranteed_downlink_rate.value_or("N/A"),
                          mapping.max_downlink_rate.value_or("N/A"));
        } else {
            logger_->warn("QoS profile mapping not found in update message for session: {}", qod_session.session_id);
        }

        logger_->debug("Parsed QoD session for update: {}", json.dump());

        // Update PCF session
        auto pcf_msg_opt = update_pcf_session(qod_session);
        if (!pcf_msg_opt) {
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                500,
                "pcf_request_creation_failed",
                "Failed to create PCF session update request",
                message->correlation_id,
                logger_);
        }
        return *pcf_msg_opt;

    } catch (const std::exception& e) {
        logger_->error("Error parsing QoD update request: {}", e.what());

        // Return error response
        return af::common::handlers::ServiceHandlerHelpers::create_error_response(
            400,
            "bad_request",
            e.what(),
            message->correlation_id,
            logger_);
    }
}

af::communication::MessagePtr QodPcfHandler::handle_qod_delete_pcf_session(
    const af::communication::MessagePtr& message) {

    logger_->info("Handling QoD delete PCF session request");

    // Parse incoming message to get session ID
    std::string session_id;
    try {
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto json = nlohmann::json::parse(payload_str);

        if (!json.contains("session_id")) {
            throw std::runtime_error("Missing required field: session_id");
        }

        session_id = json.at("session_id").get<std::string>();
        logger_->debug("Parsed session ID for deletion: {}", session_id);

        // Create a dummy QodSession with just the session_id for deletion
        af::common::qod::QodSession qod_session;
        qod_session.session_id = session_id;

        // Delete PCF session
        auto pcf_msg_opt = delete_pcf_session(qod_session);
        if (!pcf_msg_opt) {
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                500,
                "pcf_request_creation_failed",
                "Failed to create PCF session delete request",
                message->correlation_id,
                logger_);
        }
        return *pcf_msg_opt;

    } catch (const std::exception& e) {
        logger_->error("Error parsing QoD delete request: {}", e.what());

        // Return error response
        return af::common::handlers::ServiceHandlerHelpers::create_error_response(
            400,
            "bad_request",
            e.what(),
            message->correlation_id,
            logger_);
    }
}

// === PCF Session Management ===

std::optional<af::communication::MessagePtr> QodPcfHandler::create_pcf_session(
    const af::common::qod::QodSession& qod_session) {

    logger_->info("Creating PCF session for QoD session: {}", qod_session.session_id);

    try {
        // Build PCF application session request
        nlohmann::json pcf_request = build_pcf_create_request(qod_session);

        // Validate request
        logger_->debug("Constructed PCF create app session request: {}", pcf_request.dump());
        ValidationResult validation = validate_app_session_context(pcf_request);
        if (!validation.is_valid) {
            // Print validation errors
            for (const auto& err : validation.errors) {
                logger_->error("Validation error: {}", err);
            }
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                400,
                "pcf_request_invalid",
                "PCF create app session request validation failed",
                qod_session.session_id,
                logger_);
        }

        // TODO: send the request to PCF via REST client
        auto [success, response] = pcf_client_->create_app_session(pcf_request);
        if (!success) {
            logger_->error("Failed to send create app session request to PCF");
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                502,
                "pcf_request_failed",
                "Failed to send create app session request to PCF",
                qod_session.session_id,
                logger_);
        }
        logger_->info("PCF create app session request sent successfully");
        logger_->debug("PCF response: {}", response.dump());

        // Get appSessionId from response header Loacation field
        // Format is {apiRoot}/npcf-policyauthorization/v1/app-sessions/{appSessionId}
        if (!response.contains("headers") || (!response["headers"].contains("Location") && !response["headers"].contains("location"))) {
            logger_->error("PCF response missing Location header");
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                502,
                "pcf_response_invalid",
                "PCF response missing Location header",
                qod_session.session_id,
                logger_);
        }

        std::string location = response["headers"].value("Location", response["headers"].value("location", ""));

        // TODO: move this into a helper function
        // Look for the app-sessions pattern in the location header
        // Format expected: {anyBaseUrl}/npcf-policyauthorization/v1/app-sessions/{appSessionId}
        std::string app_sessions_pattern = "/app-sessions/";
        size_t app_sessions_pos = location.find(app_sessions_pattern);

        if (app_sessions_pos == std::string::npos) {
            logger_->error("PCF Location header missing '/app-sessions/' pattern: {}", location);
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                502,
                "pcf_response_invalid",
                "PCF Location header missing '/app-sessions/' pattern",
                qod_session.session_id,
                logger_);
        }

        // Extract app session ID (everything after /app-sessions/)
        size_t app_session_id_start = app_sessions_pos + app_sessions_pattern.length();
        std::string app_session_id = location.substr(app_session_id_start);

        // Validate that we actually got an app session ID
        if (app_session_id.empty()) {
            logger_->error("PCF Location header contains empty app session ID: {}", location);
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                502,
                "pcf_response_invalid",
                "PCF Location header contains empty app session ID",
                qod_session.session_id,
                logger_);
        }
        logger_->info("Extracted appSessionId from PCF response: {}", app_session_id);


        std::string pcf_session_id = app_session_id; // Use app_session_id as pcf_session_id

        // Store mapping
        store_session_mapping(qod_session.session_id, pcf_session_id,
                            PcfSessionState::CREATING);

        // The interation is synchronous send pcf_qod_session_created
        nlohmann::json response_json;
        response_json["pcf_session_id"] = pcf_session_id;
        auto msg = af::common::handlers::ServiceHandlerHelpers::create_success_response(
            response_json,
            200,
            qod_session.session_id,
            logger_);
        msg->message_type = "pcf_qod_session_created";

        // Store request for debugging
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            if (auto it = qod_to_pcf_sessions_.find(qod_session.session_id);
                it != qod_to_pcf_sessions_.end()) {
                it->second.last_request = pcf_request;
            }
        }

        logger_->debug("PCF create request: {}", pcf_request.dump());

        return msg;
    }
    catch (const std::exception& e) {
        logger_->error("Error creating PCF session request: {}", e.what());
        return std::nullopt;
    }
}

std::optional<af::communication::MessagePtr> QodPcfHandler::update_pcf_session(
    const af::common::qod::QodSession& qod_session) {

    logger_->info("Updating PCF session for QoD session: {}", qod_session.session_id);

    // Get PCF session info
    auto pcf_info_opt = get_pcf_session_info(qod_session.session_id);
    if (!pcf_info_opt || !pcf_info_opt.has_value()) {
        logger_->error("No PCF session found for QoD session: {}", qod_session.session_id);
        return std::nullopt;
    }
    auto& pcf_info = pcf_info_opt.value();

    try {
        // Build PCF update request
        nlohmann::json pcf_request;

        pcf_request["appSessionId"] = pcf_info.pcf_session_id;

        // Updated media components (for duration extension)
        pcf_request["medComponents"] = build_media_components(qod_session);

        // Update session duration
        auto mapping_opt = qod_session.qos_profile_mapping;
        if (mapping_opt && mapping_opt.has_value()) {
            auto& mapping = mapping_opt.value();
            if (mapping.max_downlink_rate) {
                pcf_request["maxReqBwDl"] = *mapping.max_downlink_rate;
            }
            if (mapping.max_uplink_rate) {
                pcf_request["maxReqBwUl"] = *mapping.max_uplink_rate;
            }
        }


        auto [success, response] = pcf_client_->update_app_session(pcf_info.pcf_session_id, pcf_request);
        if (!success) {
            logger_->error("Failed to send update app session request to PCF");
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                502,
                "pcf_request_failed",
                "Failed to send update app session request to PCF",
                qod_session.session_id,
                logger_);
        }
        logger_->info("PCF update app session request sent successfully");
        logger_->debug("PCF response: {}", response.dump());

        // Update state
        update_session_state(pcf_info.pcf_session_id, PcfSessionState::UPDATING);

        // Create message
        nlohmann::json response_json;
        response_json["pcf_session_id"] = pcf_info.pcf_session_id;
        auto msg = af::common::handlers::ServiceHandlerHelpers::create_success_response(
            response_json,
            200,
            qod_session.session_id,
            logger_);
        msg->message_type = "pcf_update_app_session";

        // Store request
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            if (auto it = qod_to_pcf_sessions_.find(qod_session.session_id);
                it != qod_to_pcf_sessions_.end()) {
                it->second.last_request = pcf_request;
            }
        }

        logger_->debug("PCF update request: {}", pcf_request.dump());

        return msg;
    }
    catch (const std::exception& e) {
        logger_->error("Error creating PCF update request: {}", e.what());
        return std::nullopt;
    }
}

std::optional<af::communication::MessagePtr> QodPcfHandler::delete_pcf_session(
    const af::common::qod::QodSession& qod_session) {

    logger_->info("Deleting PCF session for QoD session: {}", qod_session.session_id);

    // Get PCF session info
    auto pcf_info_opt = get_pcf_session_info(qod_session.session_id);
    if (!pcf_info_opt || !pcf_info_opt.has_value()) {
        logger_->warn("No PCF session found for QoD session: {}", qod_session.session_id);
        return std::nullopt;
    }
    auto& pcf_info = pcf_info_opt.value();

    try {
        // Build PCF delete request
        nlohmann::json pcf_request = nlohmann::json::object();
        pcf_request["events"] = nlohmann::json::array();
        pcf_request["events"].push_back({ {"event", "QOS_NOTIF"} });
        pcf_request["notifUri"] = af_notification_uri_ + "/" + qod_session.session_id;

        bool success = pcf_client_->delete_app_session(pcf_info.pcf_session_id, pcf_request);
        if (!success) {
            logger_->error("Failed to send delete app session request to PCF");
            return af::common::handlers::ServiceHandlerHelpers::create_error_response(
                502,
                "pcf_request_failed",
                "Failed to send delete app session request to PCF",
                qod_session.session_id,
                logger_);
        }
        logger_->info("PCF delete app session request sent successfully");

        // Update state
        update_session_state(pcf_info.pcf_session_id, PcfSessionState::DELETING);

        // Create message
        nlohmann::json response_json;
        response_json["pcf_session_id"] = pcf_info.pcf_session_id;
        auto msg = af::common::handlers::ServiceHandlerHelpers::create_success_response(
            response_json,
            200,
            qod_session.session_id,
            logger_);
        msg->message_type = "pcf_delete_app_session";

        logger_->debug("PCF delete request: {}", pcf_request.dump());

        return msg;
    }
    catch (const std::exception& e) {
        logger_->error("Error creating PCF delete request: {}", e.what());
        return std::nullopt;
    }
}

// === PCF Response Handling ===

std::pair<bool, std::string> QodPcfHandler::handle_pcf_create_response(
    const af::communication::MessagePtr& message) {

    logger_->info("Handling PCF create response");

    try {
        // Extract response payload
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto response_json = nlohmann::json::parse(payload_str);

        // Get QoD session ID from correlation
        std::string qod_session_id = message->correlation_id;

        // Check response status
        if (message->message_type == "pcf_app_session_created") {
            // Success
            std::string pcf_session_id = response_json.value("appSessionId", "");

            if (pcf_session_id.empty()) {
                logger_->error("PCF session ID missing in response");
                return {false, "Missing PCF session ID"};
            }

            // Update session state
            update_session_state(pcf_session_id, PcfSessionState::ACTIVE);

            // Store response
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                if (auto it = qod_to_pcf_sessions_.find(qod_session_id);
                    it != qod_to_pcf_sessions_.end()) {
                    it->second.last_response = response_json;
                    it->second.pcf_session_id = pcf_session_id;
                }
            }

            logger_->info("PCF session created successfully: {}", pcf_session_id);
            return {true, pcf_session_id};
        }
        else if (message->message_type == "pcf_error") {
            // Error response
            std::string error_msg = response_json.value("message", "Unknown PCF error");
            std::string error_code = response_json.value("code", "UNKNOWN");

            logger_->error("PCF creation failed: {} - {}", error_code, error_msg);

            // Remove session mapping on failure
            remove_session_mapping(qod_session_id);

            return {false, error_msg};
        }
        else {
            logger_->error("Unexpected PCF response type: {}", message->message_type);
            return {false, "Unexpected response type"};
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error handling PCF create response: {}", e.what());
        return {false, std::string("Exception: ") + e.what()};
    }
}

bool QodPcfHandler::handle_pcf_update_response(
    const af::communication::MessagePtr& message) {

    logger_->info("Handling PCF update response");

    try {
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto response_json = nlohmann::json::parse(payload_str);

        std::string qod_session_id = message->correlation_id;

        if (message->message_type == "pcf_app_session_updated") {
            // Update successful
            auto pcf_info_opt = get_pcf_session_info(qod_session_id);
            if (pcf_info_opt) {
                update_session_state(pcf_info_opt->pcf_session_id, PcfSessionState::ACTIVE);

                // Store response
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    if (auto it = qod_to_pcf_sessions_.find(qod_session_id);
                        it != qod_to_pcf_sessions_.end()) {
                        it->second.last_response = response_json;
                    }
                }
            }

            logger_->info("PCF session updated successfully");
            return true;
        }
        else {
            logger_->error("PCF update failed");
            return false;
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error handling PCF update response: {}", e.what());
        return false;
    }
}

bool QodPcfHandler::handle_pcf_delete_response(
    const af::communication::MessagePtr& message) {

    logger_->info("Handling PCF delete response");

    try {
        std::string qod_session_id = message->correlation_id;

        if (message->message_type == "pcf_app_session_deleted") {
            // Delete successful
            remove_session_mapping(qod_session_id);
            logger_->info("PCF session deleted successfully");
            return true;
        }
        else {
            logger_->error("PCF delete failed");
            return false;
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error handling PCF delete response: {}", e.what());
        return false;
    }
}

std::pair<std::optional<std::string>, std::optional<std::string>>
QodPcfHandler::handle_pcf_notification(const af::communication::MessagePtr& message) {

    logger_->info("Handling PCF notification");

    try {
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto notification_json = nlohmann::json::parse(payload_str);

        // Extract PCF session ID from notification
        std::string pcf_session_id = notification_json.value("appSessionId", "");

        if (pcf_session_id.empty()) {
            logger_->error("PCF session ID missing in notification");
            return {std::nullopt, std::nullopt};
        }

        // Get QoD session ID from mapping
        std::string qod_session_id = get_qod_session_id(pcf_session_id);
        if (qod_session_id.empty()) {
            logger_->warn("No QoD session found for PCF session: {}", pcf_session_id);
            return {std::nullopt, std::nullopt};
        }

        // Check notification type
        std::string event_type = notification_json.value("eventType", "");

        // TODO: handle different notification types
        if (event_type == "QOS_NOTIF") {
            // QoS cannot be guaranteed
            logger_->warn("PCF QoS notification for QoD session {}", qod_session_id);
            return {qod_session_id, "QOS_NOTIF"};
        }
        else {
            logger_->debug("Unhandled PCF notification type: {}", event_type);
            return {std::nullopt, std::nullopt};
        }
    }
    catch (const std::exception& e) {
        logger_->error("Error handling PCF notification: {}", e.what());
        return {std::nullopt, std::nullopt};
    }
}

// === Session Mapping ===

std::optional<PcfSessionInfo> QodPcfHandler::get_pcf_session_info(
    const std::string& qod_session_id) {

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = qod_to_pcf_sessions_.find(qod_session_id);
    if (it != qod_to_pcf_sessions_.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::string QodPcfHandler::get_qod_session_id(const std::string& pcf_session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = pcf_to_qod_sessions_.find(pcf_session_id);
    if (it != pcf_to_qod_sessions_.end()) {
        return it->second;
    }

    return "";
}

// === Translation Methods ===

nlohmann::json QodPcfHandler::build_pcf_create_request(const af::common::qod::QodSession& qod_session) {
    nlohmann::json request;

    // Application session context
    request["ascReqData"] = build_app_session_context(qod_session);

    return request;
}

nlohmann::json QodPcfHandler::build_app_session_context(const af::common::qod::QodSession& qod_session) {
    nlohmann::json context;

    // // DNN if resolved from UE state
    // if (qod_session.pdu_session_id ) {
    //     // Try to get DNN from UE state
    //     if (qod_session.ue_supi) {
    //         // auto ue_state = ue_state_manager_->get_ue_state_by_supi(*qod_session.ue_supi);
    //         if (ue_state) {
    //             for (const auto& pdu : ue_state.pdu_sessions) {
    //                 if (pdu.pdu_session_id == *qod_session.pdu_session_id) {
    //                     context["dnn"] = pdu.dnn.value;
    //                     if (pdu.snssai) {
    //                         nlohmann::json snssai;
    //                         snssai["sst"] = pdu.snssai->sst;
    //                         if (pdu.snssai->sd) {
    //                             snssai["sd"] = *pdu.snssai->sd;
    //                         }
    //                         context["sliceInfo"]["sNssai"] = snssai;
    //                     }
    //                     break;
    //                 }
    //             }
    //         }
    //     }
    // }

    // Only set when specific data is needed from PCF; omit when not required
    // context["afReqData"] = "UE_IDENTITY";

    // Media components
    context["medComponents"] = build_media_components(qod_session);

    // Subscription for notifications
    context["evSubsc"] = build_subscription_info(qod_session.session_id);

    // AF Application ID
    context["afAppId"] = af_id_;

    // == Root level fields ==
    // Service URN (optional, for specific services)
    context["servUrn"] = "urn:x-3gpp-qod:" + qod_session.qos_profile;

    // Notification URI - required field
    context["notifUri"] = af_notification_uri_ + "/" + qod_session.session_id;

    // Supported Features - required field (using placeholder value)
    context["suppFeat"] = "1"; // Basic feature support

    // UE identification - at least one of ueIpv4, ueIpv6, or ueMac is required
    if (qod_session.device) {
        const auto& device = *qod_session.device;

        // IPv4 address (highest priority)
        if (device.ipv4_address) {
            context["ueIpv4"] = device.ipv4_address->public_address.value;
        }
        // IPv6 address
        else if (device.ipv6_address) {
            context["ueIpv6"] = device.ipv6_address->value;
        }
        // Phone number as GPSI
        else if (device.phone_number) {
            context["gpsi"] = *device.phone_number;
        }
        // Network access identifier as GPSI
        else if (device.network_access_identifier) {
            context["gpsi"] = *device.network_access_identifier;
        }
    }

    // SUPI if available
    if (qod_session.ue_supi) {
        context["supi"] = qod_session.ue_supi->value;
    }

    // DNN if available (from PDU session context)
    if (qod_session.pdu_session_id) {
        // Default DNN for QoD sessions if not available from context
        context["dnn"] = "internet";
    }

    // Service information status (preliminary during creation)
    context["servInfStatus"] = "PRELIMINARY";

    return context;
}

nlohmann::json QodPcfHandler::build_media_components(
    const af::common::qod::QodSession& qod_session) {
    // TODO: Support multiple media components if needed

    nlohmann::json med_comps;
    nlohmann::json med_comp;

    // Media component number
    med_comp["medCompN"] = 1;

    // Flow status (ENABLED by default)
    med_comp["fStatus"] = "ENABLED";

    // Media type (AUDIO, VIDEO, DATA, APPLICATION, etc.)
    if (qod_session.qos_profile == "voice" || qod_session.qos_profile == "QOS_E") {
        med_comp["medType"] = "AUDIO";
    } else if (qod_session.qos_profile == "video" || qod_session.qos_profile == "QOS_S") {
        med_comp["medType"] = "VIDEO";
    } else {
        med_comp["medType"] = "APPLICATION";
    }

    // Media sub-components (flow descriptions)
    nlohmann::json med_sub_comps = map_ports_to_media_subcomponents(
        qod_session.device_ports,
        qod_session.application_server_ports);

    // Add UE-specific flow descriptions as a separate MediaSubComponent
    auto ue_specific_flows = build_flow_descriptions(qod_session);
    if (!ue_specific_flows.empty()) {
        // Find the next available flow number
        int next_flow_num = med_sub_comps.size() + 1;

        // Create a new MediaSubComponent for UE-specific flows
        nlohmann::json ue_subcomp;
        ue_subcomp["fNum"] = next_flow_num;
        ue_subcomp["fDescs"] = ue_specific_flows;  // ✓ CORRECT - fDescs inside MediaSubComponent
        ue_subcomp["fStatus"] = "ENABLED";
        ue_subcomp["flowUsage"] = "NO_INFO";  // General application data

        // Add to media sub-components
        med_sub_comps[std::to_string(next_flow_num)] = ue_subcomp;
    }

    // Assign the completed media sub-components to the media component
    med_comp["medSubComps"] = med_sub_comps;

    // Get QoS profile mapping from qod_session
    auto mapping_opt = qod_session.qos_profile_mapping;
    if (!mapping_opt || !mapping_opt.has_value()) {
        logger_->error("No mapping found for QoS profile: {}", qod_session.qos_profile);

        med_comps["1"] = med_comp;

        return med_comps; // Return with basic component
    }
    auto& mapping = mapping_opt.value();

    // QoS requirements - BitRate format should be in bits per second as string

    // 5QI reference (mutually exclusive with altSerReqsData per schema)
    if (mapping.fiveqi > 0) {
        med_comp["qosReference"] = std::to_string(mapping.fiveqi);
    }

    // Priority level (must be string format per TS 29.514: "PRIO_1", "PRIO_2", etc.)
    if (mapping.priority_level) {
        med_comp["resPrio"] = "PRIO_" + std::to_string(*mapping.priority_level);
    }

    // Bandwidth requirements per 3GPP TS 29.514
    // marBw = Maximum Requested Bandwidth (becomes MBR in 5G)
    // mirBw = Minimum Requested Bandwidth (becomes GBR in 5G for GBR QoS)
    // Values are already in TS 29.571 BitRate format: "<value> <unit>" (e.g., "128 Kbps")
    if (mapping.is_gbr) {
        // GBR QoS: Set both maximum (MBR) and guaranteed (GBR) rates

        // Maximum Requested Bandwidth (MBR) - Downlink
        if (mapping.max_downlink_rate) {
            med_comp["marBwDl"] = *mapping.max_downlink_rate;
        }

        // Maximum Requested Bandwidth (MBR) - Uplink
        if (mapping.max_uplink_rate) {
            med_comp["marBwUl"] = *mapping.max_uplink_rate;
        }

        // Minimum Requested Bandwidth (GBR) - Downlink
        if (mapping.guaranteed_downlink_rate) {
            med_comp["mirBwDl"] = *mapping.guaranteed_downlink_rate;
        }

        // Minimum Requested Bandwidth (GBR) - Uplink
        if (mapping.guaranteed_uplink_rate) {
            med_comp["mirBwUl"] = *mapping.guaranteed_uplink_rate;
        }
    } else {
        // Non-GBR QoS: Only set maximum rates (no guaranteed rates)
        if (mapping.max_downlink_rate) {
            med_comp["marBwDl"] = *mapping.max_downlink_rate;
        }
        if (mapping.max_uplink_rate) {
            med_comp["marBwUl"] = *mapping.max_uplink_rate;
        }
    }

    // Additional QoS parameters if available from mapping
    // Minimum desired bandwidth - use guaranteed rate for GBR
    if (mapping.is_gbr && mapping.guaranteed_downlink_rate) {
        med_comp["minDesBwDl"] = *mapping.guaranteed_downlink_rate;
    }
    if (mapping.is_gbr && mapping.guaranteed_uplink_rate) {
        med_comp["minDesBwUl"] = *mapping.guaranteed_uplink_rate;
    }

    // Maximum supported bandwidth - use max rate
    if (mapping.max_downlink_rate) {
        med_comp["maxSuppBwDl"] = *mapping.max_downlink_rate;
    }
    if (mapping.max_uplink_rate) {
        med_comp["maxSuppBwUl"] = *mapping.max_uplink_rate;
    }

    // Latency requirements - typical values based on 5QI
    if (mapping.fiveqi == 1 || mapping.fiveqi == 2) {
        // Conversational services (voice/video)
        med_comp["desMaxLatency"] = 100.0; // 100ms
    } else if (mapping.fiveqi == 3 || mapping.fiveqi == 4) {
        // Real-time gaming
        med_comp["desMaxLatency"] = 50.0; // 50ms
    } else if (mapping.fiveqi >= 5 && mapping.fiveqi <= 9) {
        // Non-GBR services
        med_comp["desMaxLatency"] = 300.0; // 300ms
    }

    // Packet loss requirements - typical values based on 5QI
    if (mapping.fiveqi == 1 || mapping.fiveqi == 2) {
        // Voice/Video - stricter loss requirements
        med_comp["desMaxLoss"] = 1.0; // 1% max loss
        med_comp["maxPacketLossRateDl"] = 10; // 10^-2 (1%)
        med_comp["maxPacketLossRateUl"] = 10; // 10^-2 (1%)
    } else if (mapping.fiveqi == 3 || mapping.fiveqi == 4) {
        // Real-time gaming
        med_comp["desMaxLoss"] = 0.1; // 0.1% max loss
        med_comp["maxPacketLossRateDl"] = 3; // 10^-3 (0.1%)
        med_comp["maxPacketLossRateUl"] = 3; // 10^-3 (0.1%)
    }

    // Preemption settings based on priority
    if (mapping.priority_level && *mapping.priority_level <= 15) {
        // High priority services
        med_comp["preemptCap"] = "MAY_PREEMPT";
        med_comp["preemptVuln"] = "NOT_PREEMPTABLE";
    } else {
        // Lower priority services
        med_comp["preemptCap"] = "NOT_PREEMPT";
        med_comp["preemptVuln"] = "PREEMPTABLE";
    }

    med_comps["1"] = med_comp;

    return med_comps;
}

nlohmann::json QodPcfHandler::build_flow_descriptions(const af::common::qod::QodSession& qod_session) {
    nlohmann::json flows = nlohmann::json::array();

    // Build flow description strings based on IP addresses and ports
    // Format: "permit out <protocol> from <src_ip> <src_port> to <dst_ip> <dst_port>"

    std::string protocol = "17"; // UDP by default, could be made configurable

    // Source IP (device)
    std::string src_ip = "any";
    if (qod_session.device && qod_session.device->ipv4_address) {
        src_ip = qod_session.device->ipv4_address->public_address.value;
        if (qod_session.device->ipv4_address->private_address) {
            // Include private address for NAT scenarios
            src_ip = qod_session.device->ipv4_address->private_address->value;
        }
    }

    // Destination IP (application server)
    std::string dst_ip = "any";
    if (qod_session.application_server.ipv4_address) {
        dst_ip = *qod_session.application_server.ipv4_address;
    } else if (qod_session.application_server.ipv6_address) {
        dst_ip = *qod_session.application_server.ipv6_address;
    }

    // Build flow descriptions for different port combinations
    if (qod_session.device_ports && qod_session.application_server_ports) {
        // Specific ports on both sides
        for (const auto& src_port : qod_session.device_ports->ports) {
            for (const auto& dst_port : qod_session.application_server_ports->ports) {
                std::stringstream flow;
                flow << "permit out " << protocol << " from " << src_ip
                     << " " << src_port << " to " << dst_ip << " " << dst_port;
                flows.push_back(flow.str());

                // Bidirectional - reverse flow
                std::stringstream reverse_flow;
                reverse_flow << "permit in " << protocol << " from " << dst_ip
                            << " " << dst_port << " to " << src_ip << " " << src_port;
                flows.push_back(reverse_flow.str());
            }
        }
    } else {
        // Generic flow between device and server
        std::stringstream flow;
        flow << "permit out " << protocol << " from " << src_ip << " to " << dst_ip;
        flows.push_back(flow.str());

        // Bidirectional - reverse flow
        std::stringstream reverse_flow;
        reverse_flow << "permit in " << protocol << " from " << dst_ip << " to " << src_ip;
        flows.push_back(reverse_flow.str());
    }

    return flows;
}

nlohmann::json QodPcfHandler::build_subscription_info(const std::string& qod_session_id) {
    nlohmann::json event_subscription;

    // Events to subscribe to
    event_subscription["events"] = nlohmann::json::array();
    event_subscription["events"].push_back({ {"event", "QOS_NOTIF"} });
    // TODO: add more events as needed

    // Notification URI
    event_subscription["notifUri"] = af_notification_uri_ + "/" + qod_session_id;

    // Notification correlation ID
    event_subscription["notifCorreId"] = qod_session_id;

    return event_subscription;
}

nlohmann::json QodPcfHandler::translate_device_to_ue_id(
    const std::optional<af::common::qod::QodDevice>& device,
    const std::optional<Supi>& ue_supi) {

    nlohmann::json ue_id;

    // SUPI has highest priority
    if (ue_supi) {
        ue_id["supi"] = ue_supi->value;
        return ue_id;
    }

    // Use device identifiers
    if (device) {
        if (device->phone_number) {
            // Convert phone number to GPSI format
            ue_id["gpsi"] = *device->phone_number;
        } else if (device->ipv4_address) {
            ue_id["ipv4Addr"] = device->ipv4_address->public_address.value;
        } else if (device->ipv6_address) {
            ue_id["ipv6Addr"] = device->ipv6_address->value;
        }
    }

    return ue_id;
}

nlohmann::json QodPcfHandler::build_af_request_data(const af::common::qod::QodSession& qod_session) {
    nlohmann::json af_req_data;

    // Request type

    return af_req_data;
}

nlohmann::json QodPcfHandler::map_ports_to_media_subcomponents(
    const std::optional<af::common::qod::PortsSpec>& device_ports,
    const std::optional<af::common::qod::PortsSpec>& server_ports) {

    nlohmann::json med_sub_comps = nlohmann::json::object();

    int flow_number = 1;

    // Create sub-component for each port combination
    auto add_subcomp = [&](uint16_t src_port, uint16_t dst_port, const std::string& direction) {
        nlohmann::json sub_comp;

        // Required field: fNum (flow number) - must be integer
        sub_comp["fNum"] = flow_number;

        // Flow descriptions - array of flow description strings
        nlohmann::json flow_descs = nlohmann::json::array();
        std::string protocol = "any"; // any by default

        // TODO: use the build_flow_descriptions function to generate descriptions
        // Build flow description strings based on ports
        if (src_port != 0 && dst_port != 0) {
            // Specific port flow descriptions
            std::string uplink_flow = "permit out " + protocol + " from any " +
                                    std::to_string(src_port) + " to any " + std::to_string(dst_port);
            std::string downlink_flow = "permit in " + protocol + " from any " +
                                      std::to_string(dst_port) + " to any " + std::to_string(src_port);

            flow_descs.push_back(uplink_flow);
            if (direction == "BIDIRECTIONAL") {
                flow_descs.push_back(downlink_flow);
            }
        } else {
            // Generic flow descriptions (any port)
            std::string uplink_flow = "permit out " + protocol + " from any to any";
            std::string downlink_flow = "permit in " + protocol + " from any to any";

            flow_descs.push_back(uplink_flow);
            if (direction == "BIDIRECTIONAL") {
                flow_descs.push_back(downlink_flow);
            }
        }

        // Only add flow descriptions if we have them
        if (!flow_descs.empty()) {
            sub_comp["fDescs"] = flow_descs;
        }

        // Flow status - optional
        sub_comp["fStatus"] = "ENABLED";

        // Flow usage - optional (describes the type of flow)
        if (src_port == 0 && dst_port == 0) {
            sub_comp["flowUsage"] = "NO_INFO"; // Generic flow
        } else {
            // Check for known service ports
            if (src_port == 5060 || dst_port == 5060 ||
                src_port == 5061 || dst_port == 5061) {
                sub_comp["flowUsage"] = "AF_SIGNALLING"; // SIP signaling
            } else if (src_port >= 16384 && src_port <= 32767) {
                sub_comp["flowUsage"] = "RTCP"; // Typical RTCP port range
            } else {
                sub_comp["flowUsage"] = "NO_INFO"; // General application data
            }
        }

        // TODO: Add optional fields as needed
        // Maximum bandwidth (optional) - can be set based on QoS requirements
        // These would typically come from QoS profile mapping
        // sub_comp["marBwDl"] = "1000000"; // 1 Mbps downlink
        // sub_comp["marBwUl"] = "1000000"; // 1 Mbps uplink

        // TOS/Traffic Class (optional) - for packet marking
        // Format: "XX YY" where XX is ToS/Traffic Class, YY is mask
        // sub_comp["tosTrCl"] = "46 FC"; // EF (Expedited Forwarding) marking

        // AF signaling protocol (optional) - for specific protocols
        // sub_comp["afSigProtocol"] = "SIP"; // If this is SIP signaling

        med_sub_comps[std::to_string(flow_number)] = sub_comp;
        flow_number++;
    };

    if (device_ports && server_ports) {
        // Specific port mappings
        for (const auto& dev_port : device_ports->ports) {
            for (const auto& srv_port : server_ports->ports) {
                add_subcomp(dev_port, srv_port, "BIDIRECTIONAL");
            }
        }

        // Port ranges - create representative flows for ranges
        for (const auto& dev_range : device_ports->ranges) {
            for (const auto& srv_range : server_ports->ranges) {
                // Add flow for start of range
                add_subcomp(dev_range.from, srv_range.from, "BIDIRECTIONAL");

                // Optionally add flow for end of range if range is large
                if ((dev_range.to - dev_range.from) > 10 ||
                    (srv_range.to - srv_range.from) > 10) {
                    add_subcomp(dev_range.to, srv_range.to, "BIDIRECTIONAL");
                }
            }
        }
    }
    else if (device_ports) {
        // Only device ports specified
        for (const auto& dev_port : device_ports->ports) {
            add_subcomp(dev_port, 0, "BIDIRECTIONAL");
        }
        for (const auto& dev_range : device_ports->ranges) {
            add_subcomp(dev_range.from, 0, "BIDIRECTIONAL");
        }
    }
    else if (server_ports) {
        // Only server ports specified
        for (const auto& srv_port : server_ports->ports) {
            add_subcomp(0, srv_port, "BIDIRECTIONAL");
        }
        for (const auto& srv_range : server_ports->ranges) {
            add_subcomp(0, srv_range.from, "BIDIRECTIONAL");
        }
    }
    else {
        // No specific ports - create default bidirectional flow
        add_subcomp(0, 0, "BIDIRECTIONAL");
    }

    return med_sub_comps;
}

void QodPcfHandler::store_session_mapping(const std::string& qod_session_id,
                                          const std::string& pcf_session_id,
                                          PcfSessionState state) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    PcfSessionInfo info;
    info.pcf_session_id = pcf_session_id;
    info.qod_session_id = qod_session_id;
    info.state = state;
    info.created_at = std::chrono::system_clock::now();

    qod_to_pcf_sessions_[qod_session_id] = info;
    pcf_to_qod_sessions_[pcf_session_id] = qod_session_id;

    logger_->debug("Stored session mapping: QoD {} -> PCF {}", qod_session_id, pcf_session_id);
}

void QodPcfHandler::update_session_state(const std::string& pcf_session_id,
                                         PcfSessionState state) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto pcf_it = pcf_to_qod_sessions_.find(pcf_session_id);
    if (pcf_it != pcf_to_qod_sessions_.end()) {
        auto qod_it = qod_to_pcf_sessions_.find(pcf_it->second);
        if (qod_it != qod_to_pcf_sessions_.end()) {
            qod_it->second.state = state;
            logger_->debug("Updated PCF session {} state to {}", pcf_session_id,
                          static_cast<int>(state));
        }
    }
}

void QodPcfHandler::remove_session_mapping(const std::string& qod_session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = qod_to_pcf_sessions_.find(qod_session_id);
    if (it != qod_to_pcf_sessions_.end()) {
        std::string pcf_session_id = it->second.pcf_session_id;

        qod_to_pcf_sessions_.erase(it);
        pcf_to_qod_sessions_.erase(pcf_session_id);

        logger_->debug("Removed session mapping for QoD session: {}", qod_session_id);
    }
}

// TODO: check if this should be moved to a common utility file so that NEF Handler can also use it
std::optional<af::common::qod::QodDevice> QodPcfHandler::parse_device(const nlohmann::json& json) {
    if (json.is_null()) {
        return std::nullopt;
    }

    af::common::qod::QodDevice device;

    if (json.contains("gpsi")) {
        device.phone_number = json["gpsi"];
    }

    if (json.contains("supi")) {
        device.network_access_identifier = json["supi"];
    }

    if (json.contains("ipv4Address")) {
        logger_->debug("Parsing IPv4 address from JSON {}", json["ipv4Address"].dump());

        af::common::qod::DeviceIpv4Addr ipv4;
        auto& ipv4_json = json["ipv4Address"];

        if (ipv4_json.contains("publicAddress")) {
            ipv4.public_address = Ipv4Addr{ipv4_json["publicAddress"]};
        }

        if (ipv4_json.contains("privateAddress")) {
            ipv4.private_address = Ipv4Addr{ipv4_json["privateAddress"]};
        }

        if (ipv4_json.contains("publicPort")) {
            ipv4.public_port = ipv4_json["publicPort"].get<af::common::qod::Port>();
        }

        device.ipv4_address = ipv4;
    }

    // TODO: check if ipv6 is being handled correctly
    if (json.contains("ipv6Address")) {
        device.ipv6_address = Ipv6Addr{json["ipv6Address"]};
    }

    return device;
}

af::common::qod::ApplicationServer QodPcfHandler::parse_application_server(const nlohmann::json& json) {
    af::common::qod::ApplicationServer server;

    if (json.contains("ipv4Address")) {
        server.ipv4_address = json["ipv4Address"];
    }

    if (json.contains("ipv6Address")) {
        server.ipv6_address = json["ipv6Address"];
    }

    return server;
}

std::optional<af::common::qod::PortsSpec> QodPcfHandler::parse_ports(const nlohmann::json& json) {
    if (json.is_null()) {
        return std::nullopt;
    }

    af::common::qod::PortsSpec ports;

    if (json.contains("ranges") && json["ranges"].is_array()) {
        for (const auto& range_json : json["ranges"]) {
            af::common::qod::PortRange range;
            range.from = range_json["from"].get<af::common::qod::Port>();
            range.to = range_json["to"].get<af::common::qod::Port>();
            ports.ranges.push_back(range);
        }
    }

    if (json.contains("ports") && json["ports"].is_array()) {
        for (const auto& port_json : json["ports"]) {
            ports.ports.push_back(port_json.get<af::common::qod::Port>());
        }
    }

    return ports;
}

std::optional<af::common::qod::SinkCredential> QodPcfHandler::parse_sink_credential(const nlohmann::json& json) {
    if (json.is_null()) {
        return std::nullopt;
    }

    af::common::qod::SinkCredential credential;

    std::string type = json["credentialType"];
    if (type == "ACCESSTOKEN") {
        credential.credential_type = af::common::qod::SinkCredential::CredentialType::ACCESSTOKEN;

        if (json.contains("accessToken")) {
            credential.access_token = json["accessToken"];
        }

        if (json.contains("accessTokenExpiresUtc")) {
            // Parse RFC3339 timestamp
            std::string time_str = json["accessTokenExpiresUtc"];
            std::tm tm = {};
            std::stringstream ss(time_str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            credential.access_token_expires_utc =
                std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }

        if (json.contains("accessTokenType")) {
            credential.access_token_type = json["accessTokenType"];
        }
    } else if (type == "PLAIN") {
        credential.credential_type = af::common::qod::SinkCredential::CredentialType::PLAIN;
        credential.identifier = json.value("identifier", "");
        credential.secret = json.value("secret", "");
    } else if (type == "REFRESHTOKEN") {
        credential.credential_type = af::common::qod::SinkCredential::CredentialType::REFRESHTOKEN;
        credential.access_token = json.value("accessToken", "");
        credential.refresh_token = json.value("refreshToken", "");
        credential.refresh_token_endpoint = json.value("refreshTokenEndpoint", "");
    }

    return credential;
}



} // namespace southbound
} // namespace af