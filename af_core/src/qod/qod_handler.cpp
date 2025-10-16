/**
 * @file qod_handler.cpp
 * @brief Implementation of the QoD handler
 */

#include "qod/qod_handler.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <regex>
#include <chrono>
#include <iomanip>
#include "af_orchestrator.h"

namespace af {
namespace qod {

QodHandler::QodHandler(std::shared_ptr<QodSessionManager> session_manager)
    : session_manager_(session_manager) {
    
    // Setup logger
    initializeLogger(spdlog::level::debug);
    
    logger_->info("QoD Handler created");
}

QodHandler::~QodHandler() {
    // Clean up resources
}

void QodHandler::initialize(af::core::AfOrchestrator* orchestrator) {
    orchestrator_ = orchestrator;
    logger_->info("QoD Handler initialized");
}

void QodHandler::initializeLogger(spdlog::level::level_enum log_level) {
    logger_ = spdlog::get("qod_handler");
    
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("qod_handler");
    }
    
    logger_->set_level(log_level);
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

// === CAMARA QoD API Handlers ===

af::communication::MessagePtr QodHandler::handle_create_session(
    const af::communication::MessagePtr& message) {
    
    logger_->info("Handling create session request");
    
    auto context = extract_context(message);
    
    try {
        // Parse request body
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto request_json = nlohmann::json::parse(payload_str);
        
        logger_->debug("Create session request: {}", request_json.dump());
        
        // Create session request
        af::common::qod::CreateSessionRequest request;
        request.api_consumer_id = context.api_consumer_id;
        request.correlation_id = context.correlation_id;
        
        // Parse device
        if (request_json.contains("device")) {
            request.device = parse_device(request_json["device"]);
            
            // Validate device based on token type
            auto device_error = validate_device(request.device, context);
            if (device_error) {
                logger_->error("Device validation error: {}", device_error.value());
                return create_error_response(422, 
                    context.is_three_legged ? ErrorCode::UNNECESSARY_IDENTIFIER : ErrorCode::MISSING_IDENTIFIER,
                    *device_error, context.correlation_id);
            }
        } else if (!context.is_three_legged) {
            // 2-legged token requires device
            return create_error_response(422, ErrorCode::MISSING_IDENTIFIER,
                "Device identifier is required for 2-legged authentication",
                context.correlation_id);
        }
        
        // Parse application server (required)
        if (!request_json.contains("applicationServer")) {
            return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
                "applicationServer is required", context.correlation_id);
        }
        request.application_server = parse_application_server(request_json["applicationServer"]);
        
        auto server_error = validate_application_server(request.application_server);
        if (server_error) {
            return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
                *server_error, context.correlation_id);
        }
        
        // Parse ports (optional)
        if (request_json.contains("devicePorts")) {
            request.device_ports = parse_ports(request_json["devicePorts"]);
            auto ports_error = validate_ports(request.device_ports);
            if (ports_error) {
                return create_error_response(400, ErrorCode::OUT_OF_RANGE,
                    *ports_error, context.correlation_id);
            }
        }
        
        if (request_json.contains("applicationServerPorts")) {
            request.application_server_ports = parse_ports(request_json["applicationServerPorts"]);
            auto ports_error = validate_ports(request.application_server_ports);
            if (ports_error) {
                return create_error_response(400, ErrorCode::OUT_OF_RANGE,
                    *ports_error, context.correlation_id);
            }
        }
        
        // Parse QoS profile (required)
        if (!request_json.contains("qosProfile")) {
            return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
                "qosProfile is required", context.correlation_id);
        }
        request.qos_profile = request_json["qosProfile"];
        
        // Parse duration (required)
        if (!request_json.contains("duration")) {
            return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
                "duration is required", context.correlation_id);
        }
        request.duration = std::chrono::seconds(request_json["duration"].get<int>());
        
        auto duration_error = validate_duration(request.duration, request.qos_profile);
        if (duration_error) {
            return create_error_response(400, ErrorCode::DURATION_OUT_OF_RANGE,
                *duration_error, context.correlation_id);
        }
        
        // Parse notification sink (optional)
        if (request_json.contains("sink")) {
            request.sink = request_json["sink"];
            
            if (request_json.contains("sinkCredential")) {
                request.sink_credential = parse_sink_credential(request_json["sinkCredential"]);
            }
            
            auto sink_error = validate_sink(request.sink, request.sink_credential);
            if (sink_error) {
                return create_error_response(400, ErrorCode::INVALID_SINK,
                    *sink_error, context.correlation_id);
            }
        }
        
        // Create session via manager
        auto session = session_manager_->create_session(request);
        
        if (!session) {
            // Check reason for failure
            // TODO: Get more specific error from session manager
            return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
                "Failed to create session - possible conflict with existing session",
                context.correlation_id);
        }
        
        // Determine if device should be included in response
        bool include_device = request.device.has_value() && !context.is_three_legged;
        
        // Convert to JSON response
        auto response_json = session_to_json(*session, include_device);
        
        logger_->info("Session created successfully: {}", session->session_id);
        
        return create_success_response(response_json, 201, context.correlation_id);
    }
    catch (const nlohmann::json::parse_error& e) {
        logger_->error("JSON parse error: {}", e.what());
        return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
            "Invalid JSON in request body", context.correlation_id);
    }
    catch (const std::exception& e) {
        logger_->error("Error handling create session: {}", e.what());
        return create_error_response(500, "INTERNAL_ERROR",
            "Internal server error", context.correlation_id);
    }
}

af::communication::MessagePtr QodHandler::handle_get_session(
    const af::communication::MessagePtr& message) {
    
    logger_->info("Handling get session request");
    
    auto context = extract_context(message);
    auto session_id = extract_session_id(message);
    
    if (session_id.empty()) {
        return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
            "Session ID is required", context.correlation_id);
    }
    
    try {
        // Get session from manager
        auto session = session_manager_->get_session(session_id, context.api_consumer_id);
        
        if (!session) {
            return create_error_response(404, ErrorCode::NOT_FOUND,
                "Session not found", context.correlation_id);
        }
        
        // Determine if device should be included in response
        bool include_device = session->device.has_value() && 
                             session->device_response.has_value();
        
        // Convert to JSON response
        auto response_json = session_to_json(*session, include_device);
        
        logger_->info("Session retrieved successfully: {}", session_id);
        
        return create_success_response(response_json, 200, context.correlation_id);
    }
    catch (const std::exception& e) {
        logger_->error("Error handling get session: {}", e.what());
        return create_error_response(500, "INTERNAL_ERROR",
            "Internal server error", context.correlation_id);
    }
}

af::communication::MessagePtr QodHandler::handle_delete_session(
    const af::communication::MessagePtr& message) {
    
    logger_->info("Handling delete session request");
    
    auto context = extract_context(message);
    auto session_id = extract_session_id(message);
    
    if (session_id.empty()) {
        return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
            "Session ID is required", context.correlation_id);
    }
    
    try {
        // Delete session via manager
        bool deleted = session_manager_->delete_session(session_id, context.api_consumer_id);
        
        if (!deleted) {
            return create_error_response(400, ErrorCode::NOT_FOUND,
                "Session deletion failed", context.correlation_id);
        }
        
        logger_->info("Session deleted successfully: {}", session_id);
        
        // Return 204 No Content
        auto response = std::make_shared<af::communication::Message>();
        response->message_type = "qod_delete_session_response";
        response->correlation_id = context.correlation_id;
        
        // Set HTTP status in metadata        
        response->metadata["http_status"] = "204";
        response->metadata["x-correlator"] = context.correlation_id;
        
        return response;
    }
    catch (const std::exception& e) {
        logger_->error("Error handling delete session: {}", e.what());
        return create_error_response(500, "INTERNAL_ERROR",
            "Internal server error", context.correlation_id);
    }
}

af::communication::MessagePtr QodHandler::handle_extend_session(
    const af::communication::MessagePtr& message) {
    
    logger_->info("Handling extend session request");
    
    auto context = extract_context(message);
    auto session_id = extract_session_id(message);
    
    if (session_id.empty()) {
        return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
            "Session ID is required", context.correlation_id);
    }
    
    try {
        // Parse request body
        std::string payload_str(message->payload.begin(), message->payload.end());
        auto request_json = nlohmann::json::parse(payload_str);
        
        logger_->debug("Extend session request: {}", request_json.dump());
        
        // Create extension request
        af::common::qod::ExtendSessionDurationRequest request;
        request.session_id = session_id;
        
        if (!request_json.contains("requestedAdditionalDuration")) {
            return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
                "requestedAdditionalDuration is required", context.correlation_id);
        }
        
        request.requested_additional_duration = 
            std::chrono::seconds(request_json["requestedAdditionalDuration"].get<int>());
        
        if (request.requested_additional_duration.count() < 1) {
            return create_error_response(400, ErrorCode::OUT_OF_RANGE,
                "requestedAdditionalDuration must be at least 1 second",
                context.correlation_id);
        }
        
        // Extend session via manager
        auto session = session_manager_->extend_session_duration(request, context.api_consumer_id);
        
        if (!session) {
            // Could be not found, wrong state, or authorization failure
            // Try to get session to determine specific error
            auto existing = session_manager_->get_session(session_id, context.api_consumer_id);
            if (!existing) {
                return create_error_response(404, ErrorCode::NOT_FOUND,
                    "Session not found", context.correlation_id);
            }
            
            if (existing->qos_status != af::common::qod::QosStatus::AVAILABLE) {
                return create_error_response(409, ErrorCode::SESSION_EXTENSION_NOT_ALLOWED,
                    "Extending the session duration is not allowed in the current state (" +
                    af::common::qod::QodTypeUtils::qos_status_to_string(existing->qos_status) + 
                    "). The session must be in the AVAILABLE state.",
                    context.correlation_id);
            }
            
            return create_error_response(400, ErrorCode::DURATION_OUT_OF_RANGE,
                "Extension would exceed maximum duration for QoS profile",
                context.correlation_id);
        }
        
        // Determine if device should be included in response
        bool include_device = session->device.has_value() && 
                             session->device_response.has_value();
        
        // Convert to JSON response
        auto response_json = session_to_json(*session, include_device);
        
        logger_->info("Session extended successfully: {}", session_id);
        
        return create_success_response(response_json, 200, context.correlation_id);
    }
    catch (const nlohmann::json::parse_error& e) {
        logger_->error("JSON parse error: {}", e.what());
        return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
            "Invalid JSON in request body", context.correlation_id);
    }
    catch (const std::exception& e) {
        logger_->error("Error handling extend session: {}", e.what());
        return create_error_response(500, "INTERNAL_ERROR",
            "Internal server error", context.correlation_id);
    }
}

af::communication::MessagePtr QodHandler::handle_retrieve_sessions(
    const af::communication::MessagePtr& message) {
    
    logger_->info("Handling retrieve sessions request");
    
    auto context = extract_context(message);
    
    try {
        // Parse request body
        std::string payload_str(message->payload.begin(), message->payload.end());
        
        af::common::qod::RetrieveSessionsRequest request;
        request.api_consumer_id = context.api_consumer_id;
        
        // Handle optional body for device specification
        if (!payload_str.empty()) {
            auto request_json = nlohmann::json::parse(payload_str);
            
            if (request_json.contains("device")) {
                request.device = parse_device(request_json["device"]);
                
                // Validate device based on token type
                auto device_error = validate_device(request.device, context);
                if (device_error) {
                    return create_error_response(422,
                        context.is_three_legged ? ErrorCode::UNNECESSARY_IDENTIFIER : ErrorCode::MISSING_IDENTIFIER,
                        *device_error, context.correlation_id);
                }
            }
        } else if (!context.is_three_legged) {
            // 2-legged token requires device
            return create_error_response(422, ErrorCode::MISSING_IDENTIFIER,
                "Device identifier is required for 2-legged authentication",
                context.correlation_id);
        }
        
        // Retrieve sessions via manager
        auto sessions = session_manager_->retrieve_sessions_by_device(request);
        
        // Convert to JSON array response
        nlohmann::json response_json = nlohmann::json::array();
        for (const auto& session : sessions) {
            bool include_device = session.device.has_value() && 
                                 session.device_response.has_value();
            response_json.push_back(session_to_json(session, include_device));
        }
        
        logger_->info("Retrieved {} sessions for device", sessions.size());
        
        return create_success_response(response_json, 200, context.correlation_id);
    }
    catch (const nlohmann::json::parse_error& e) {
        logger_->error("JSON parse error: {}", e.what());
        return create_error_response(400, ErrorCode::INVALID_ARGUMENT,
            "Invalid JSON in request body", context.correlation_id);
    }
    catch (const std::exception& e) {
        logger_->error("Error handling retrieve sessions: {}", e.what());
        return create_error_response(500, "INTERNAL_ERROR",
            "Internal server error", context.correlation_id);
    }
}

// === Validation Methods ===

std::optional<std::string> QodHandler::validate_device(
    const std::optional<af::common::qod::QodDevice>& device,
    const QodRequestContext& context) {
    
    if (context.is_three_legged && device) {
        return "Device must not be provided with 3-legged authentication";
    }
    
    if (!context.is_three_legged && !device) {
        return "Device is required for 2-legged authentication";
    }
    
    if (device) {
        // Validate phone number format
        if (device->phone_number) {
            std::regex phone_regex("^\\+[1-9][0-9]{4,14}$");
            if (!std::regex_match(*device->phone_number, phone_regex)) {
                return "Invalid phone number format - must be E.164 format with + prefix";
            }
        }
        
        // Validate IPv4 address
        if (device->ipv4_address) {
            if (device->ipv4_address->public_address.value.empty()) {
                return "Public IPv4 address is required when using IPv4 identification";
            }
            
            if (!device->ipv4_address->private_address && !device->ipv4_address->public_port) {
                return "Either private address or public port must be provided with public IPv4 address";
            }
        }
        
        // Ensure at least one identifier is provided
        if (!device->phone_number && !device->ipv4_address && 
            !device->ipv6_address && !device->network_access_identifier) {
            return "At least one device identifier must be provided";
        }
    }
    
    return std::nullopt;
}

std::optional<std::string> QodHandler::validate_application_server(
    const af::common::qod::ApplicationServer& app_server) {
    
    if (app_server.is_empty()) {
        return "Application server must have at least one IP address";
    }
    
    // TODO: Validate IP address formats and CIDR notation
    
    return std::nullopt;
}

std::optional<std::string> QodHandler::validate_ports(
    const std::optional<af::common::qod::PortsSpec>& ports) {
    
    if (!ports) {
        return std::nullopt;
    }
    
    // Validate port ranges
    for (const auto& range : ports->ranges) {
        if (range.from > range.to) {
            return "Invalid port range - 'from' must be less than or equal to 'to'";
        }
        if (range.from > 65535 || range.to > 65535) {
            return "Port numbers must be between 0 and 65535";
        }
    }
    
    // Validate individual ports
    for (const auto& port : ports->ports) {
        if (port > 65535) {
            return "Port numbers must be between 0 and 65535";
        }
    }
    
    return std::nullopt;
}

std::optional<std::string> QodHandler::validate_duration(
    std::chrono::seconds duration,
    const std::string& qos_profile) {
    
    if (duration.count() < 1) {
        return "Duration must be at least 1 second";
    }
    
    // Additional validation would be done in session manager
    // based on QoS profile limits
    
    return std::nullopt;
}

std::optional<std::string> QodHandler::validate_sink(
    const std::optional<std::string>& sink,
    const std::optional<af::common::qod::SinkCredential>& credential) {
    
    if (!sink) {
        return std::nullopt;
    }
    
    // Validate sink URL format (must be HTTPS)
    std::regex url_regex("^https://[^\\s]+$");
    if (!std::regex_match(*sink, url_regex)) {
        return "Sink must be a valid HTTPS URL";
    }
    
    // Validate credential if provided
    if (credential) {
        if (credential->credential_type != af::common::qod::SinkCredential::CredentialType::ACCESSTOKEN) {
            return "Only ACCESSTOKEN credential type is supported";
        }
        
        if (!credential->access_token || credential->access_token->empty()) {
            return "Access token is required for ACCESSTOKEN credential type";
        }
        
        if (!credential->access_token_expires_utc) {
            return "Access token expiry time is required";
        }
        
        if (credential->access_token_type != "bearer") {
            return "Access token type must be 'bearer'";
        }
        
        // Check if token is not already expired
        if (*credential->access_token_expires_utc < std::chrono::system_clock::now()) {
            return "Access token has already expired";
        }
    }
    
    return std::nullopt;
}

// === JSON Conversion Methods ===

std::optional<af::common::qod::QodDevice> QodHandler::parse_device(const nlohmann::json& json) {
    if (json.is_null()) {
        return std::nullopt;
    }
    
    af::common::qod::QodDevice device;
    
    if (json.contains("phoneNumber")) {
        device.phone_number = json["phoneNumber"];
    }
    
    if (json.contains("networkAccessIdentifier")) {
        device.network_access_identifier = json["networkAccessIdentifier"];
    }
    
    if (json.contains("ipv4Address")) {
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
    
    if (json.contains("ipv6Address")) {
        device.ipv6_address = Ipv6Addr{json["ipv6Address"]};
    }
    
    return device;
}

// TODO: remove this has been moved to southbound/pcf_handler/src/qod_pcf_handler.cpp
af::common::qod::ApplicationServer QodHandler::parse_application_server(const nlohmann::json& json) {
    af::common::qod::ApplicationServer server;
    
    if (json.contains("ipv4Address")) {
        server.ipv4_address = json["ipv4Address"];
    }
    
    if (json.contains("ipv6Address")) {
        server.ipv6_address = json["ipv6Address"];
    }
    
    return server;
}

std::optional<af::common::qod::PortsSpec> QodHandler::parse_ports(const nlohmann::json& json) {
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

std::optional<af::common::qod::SinkCredential> QodHandler::parse_sink_credential(const nlohmann::json& json) {
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

nlohmann::json QodHandler::session_to_json(
    const af::common::qod::QodSession& session,
    bool include_device) {
    
    nlohmann::json result;
    
    // Session ID and status
    result["sessionId"] = session.session_id;
    result["qosStatus"] = af::common::qod::QodTypeUtils::qos_status_to_string(session.qos_status);
    
    // QoS profile
    result["qosProfile"] = session.qos_profile;
    
    // Duration
    result["duration"] = session.duration.count();
    
    // Application server
    nlohmann::json app_server_json;
    if (session.application_server.ipv4_address) {
        app_server_json["ipv4Address"] = *session.application_server.ipv4_address;
    }
    if (session.application_server.ipv6_address) {
        app_server_json["ipv6Address"] = *session.application_server.ipv6_address;
    }
    result["applicationServer"] = app_server_json;
    
    // Device (only if requested and available)
    if (include_device && session.device_response) {
        result["device"] = device_to_json(*session.device_response);
    }
    
    // Ports
    if (session.device_ports) {
        nlohmann::json ports_json;
        if (!session.device_ports->ranges.empty()) {
            nlohmann::json ranges = nlohmann::json::array();
            for (const auto& range : session.device_ports->ranges) {
                ranges.push_back({{"from", range.from}, {"to", range.to}});
            }
            ports_json["ranges"] = ranges;
        }
        if (!session.device_ports->ports.empty()) {
            ports_json["ports"] = session.device_ports->ports;
        }
        result["devicePorts"] = ports_json;
    }
    
    if (session.application_server_ports) {
        nlohmann::json ports_json;
        if (!session.application_server_ports->ranges.empty()) {
            nlohmann::json ranges = nlohmann::json::array();
            for (const auto& range : session.application_server_ports->ranges) {
                ranges.push_back({{"from", range.from}, {"to", range.to}});
            }
            ports_json["ranges"] = ranges;
        }
        if (!session.application_server_ports->ports.empty()) {
            ports_json["ports"] = session.application_server_ports->ports;
        }
        result["applicationServerPorts"] = ports_json;
    }
    
    // Notification sink
    if (session.sink) {
        result["sink"] = *session.sink;
    }
    
    // Timing information for AVAILABLE/UNAVAILABLE sessions
    if (session.qos_status != af::common::qod::QosStatus::REQUESTED) {
        if (session.started_at) {
            // Format as RFC3339
            auto time_t = std::chrono::system_clock::to_time_t(*session.started_at);
            char buffer[100];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t));
            result["startedAt"] = std::string(buffer);
        }
        
        if (session.expires_at) {
            auto time_t = std::chrono::system_clock::to_time_t(*session.expires_at);
            char buffer[100];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t));
            result["expiresAt"] = std::string(buffer);
        }
    }
    
    // Status info for UNAVAILABLE sessions
    if (session.qos_status == af::common::qod::QosStatus::UNAVAILABLE && session.status_info) {
        result["statusInfo"] = af::common::qod::QodTypeUtils::status_info_to_string(*session.status_info);
    }
    
    return result;
}

nlohmann::json QodHandler::device_to_json(const af::common::qod::QodDevice& device) {
    nlohmann::json result;
    
    if (device.phone_number) {
        result["phoneNumber"] = *device.phone_number;
    }
    
    if (device.network_access_identifier) {
        result["networkAccessIdentifier"] = *device.network_access_identifier;
    }
    
    if (device.ipv4_address) {
        nlohmann::json ipv4_json;
        ipv4_json["publicAddress"] = device.ipv4_address->public_address.value;
        
        if (device.ipv4_address->private_address) {
            ipv4_json["privateAddress"] = device.ipv4_address->private_address->value;
        }
        
        if (device.ipv4_address->public_port) {
            ipv4_json["publicPort"] = *device.ipv4_address->public_port;
        }
        
        result["ipv4Address"] = ipv4_json;
    }
    
    if (device.ipv6_address) {
        result["ipv6Address"] = device.ipv6_address->value;
    }
    
    return result;
}

// === Error Response Methods ===

af::communication::MessagePtr QodHandler::create_error_response(
    int status,
    const std::string& code,
    const std::string& message,
    const std::string& correlation_id) {
    
    nlohmann::json error_json;
    error_json["status"] = status;
    error_json["code"] = code;
    error_json["message"] = message;
    
    auto response = std::make_shared<af::communication::Message>();
    response->message_type = "qod_error_response";
    response->correlation_id = correlation_id;
    
    std::string payload = error_json.dump();
    response->payload.assign(payload.begin(), payload.end());
    
    // Add status code to metadata (can be HTTP/gRPC status or custom)
    // TODO: create AF specific error codes, these will be mapped to HTTP/gRPC codes at API gateway layer
    response->metadata["status"] = std::to_string(status);
    response->metadata["x-correlator"] = correlation_id;
    
    logger_->debug("Error response: {} - {} - {}", status, code, message);
    
    return response;
}

af::communication::MessagePtr QodHandler::create_success_response(
    const nlohmann::json& data,
    int status,
    const std::string& correlation_id) {
    
    auto response = std::make_shared<af::communication::Message>();
    response->message_type = "qod_success_response";
    response->correlation_id = correlation_id;
    
    std::string payload = data.dump();
    response->payload.assign(payload.begin(), payload.end());

    // Add status code to metadata (can be HTTP/gRPC status or custom)
    // TODO: create AF specific error codes, these will be mapped to HTTP/gRPC codes at API gateway layer
    response->metadata["status"] = std::to_string(status);
    response->metadata["x-correlator"] = correlation_id;
    
    return response;
}

QodRequestContext QodHandler::extract_context(const af::communication::MessagePtr& message) {
    QodRequestContext context;
    
    // Extract correlation ID
    context.correlation_id = message->correlation_id;
    if (context.correlation_id.empty()) {
        // Generate one if not provided
        context.correlation_id = "qod-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    // Extract metadata
    if (!message->metadata.empty()) {
        try {
            
            // message->metadata is a map<string, string>, convert to JSON string
            nlohmann::json metadata_json = message->metadata;
            // Extract API consumer ID (from OAuth token or API key)
            if (metadata_json.contains("api_consumer_id")) {
                context.api_consumer_id = metadata_json["api_consumer_id"];
            } else {
                // Default for testing
                context.api_consumer_id = "default_consumer";
            }
            
            // Determine if 3-legged authentication
            if (metadata_json.contains("auth_type")) {
                context.is_three_legged = (metadata_json["auth_type"] == "3-legged");
            } else if (metadata_json.contains("token_type")) {
                context.is_three_legged = (metadata_json["token_type"] == "user");
            }
            
            // Extract device from token if 3-legged
            if (context.is_three_legged && metadata_json.contains("device_id")) {
                context.device_from_token = metadata_json["device_id"];
            }
            
            // Extract x-correlator header if provided
            if (metadata_json.contains("x-correlator")) {
                context.correlation_id = metadata_json["x-correlator"];
            }
        }
        catch (const std::exception& e) {
            logger_->warn("Failed to parse message metadata: {}", e.what());
        }
    }
    
    return context;
}

std::string QodHandler::extract_session_id(const af::communication::MessagePtr& message) {
    // Extract session ID from message metadata or path
    if (!message->metadata.empty()) {
        try {
            nlohmann::json metadata_json = message->metadata;
            
            if (metadata_json.contains("session_id")) {
                return metadata_json["session_id"];
            }
            
            // Try to extract from path parameter
            if (metadata_json.contains("path_params") && 
                metadata_json["path_params"].contains("sessionId")) {
                return metadata_json["path_params"]["sessionId"];
            }
        }
        catch (const std::exception& e) {
            logger_->warn("Failed to extract session ID from metadata: {}", e.what());
        }
    }
    
    return "";
}

} // namespace qod
} // namespace af