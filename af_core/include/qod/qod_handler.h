/**
 * @file qod_handler.h
 * @brief Handles CAMARA QualityOnDemand API requests
 *
 * This class handles incoming CAMARA QoD API requests, validates them,
 * and coordinates with the QodSessionManager for session management.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "qod_session_manager.h"
#include "qod_error_codes.h"
#include "../../common/communication/include/message.h"
#include "../../common/models/qod/qod_session.h"

namespace af {
namespace core {
    class AfOrchestrator; // Forward declaration
}

namespace qod {

/**
 * @brief Request context for QoD operations
 */
struct QodRequestContext {
    std::string api_consumer_id;      // API consumer identifier
    std::string correlation_id;       // X-Correlator header value
    bool is_three_legged;             // Whether using 3-legged OAuth
    std::optional<std::string> device_from_token; // Device identified from token
};

/**
 * @brief Handles CAMARA QualityOnDemand API requests
 */
class QodHandler {
public:
    /**
     * @brief Constructor
     * @param session_manager Shared pointer to QoD session manager
     */
    explicit QodHandler(std::shared_ptr<QodSessionManager> session_manager);

    /**
     * @brief Destructor
     */
    ~QodHandler();

    /**
     * @brief Initialize the QoD handler
     * @param orchestrator Pointer to the orchestrator
     */
    void initialize(af::core::AfOrchestrator* orchestrator);

    // === CAMARA QoD API Handlers ===

    /**
     * @brief Handle POST /sessions (create session)
     * @param message Incoming request message
     * @return Response message
     */
    af::communication::MessagePtr handle_create_session(
        const af::communication::MessagePtr& message);

    /**
     * @brief Handle GET /sessions/{sessionId} (get session)
     * @param message Incoming request message
     * @return Response message
     */
    af::communication::MessagePtr handle_get_session(
        const af::communication::MessagePtr& message);

    /**
     * @brief Handle DELETE /sessions/{sessionId} (delete session)
     * @param message Incoming request message
     * @return Response message
     */
    af::communication::MessagePtr handle_delete_session(
        const af::communication::MessagePtr& message);

    /**
     * @brief Handle POST /sessions/{sessionId}/extend (extend session)
     * @param message Incoming request message
     * @return Response message
     */
    af::communication::MessagePtr handle_extend_session(
        const af::communication::MessagePtr& message);

    /**
     * @brief Handle POST /retrieve-sessions (get sessions by device)
     * @param message Incoming request message
     * @return Response message
     */
    af::communication::MessagePtr handle_retrieve_sessions(
        const af::communication::MessagePtr& message);

    /**
     * @brief Initialize logger
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // === Validation Methods ===

    /**
     * @brief Validate device identifier
     * @param device Device to validate
     * @param context Request context
     * @return Error message if invalid, empty if valid
     */
    std::optional<std::string> validate_device(
        const std::optional<af::common::qod::QodDevice>& device,
        const QodRequestContext& context);

    /**
     * @brief Validate application server
     * @param app_server Application server to validate
     * @return Error message if invalid, empty if valid
     */
    std::optional<std::string> validate_application_server(
        const af::common::qod::ApplicationServer& app_server);

    /**
     * @brief Validate port specification
     * @param ports Ports to validate
     * @return Error message if invalid, empty if valid
     */
    std::optional<std::string> validate_ports(
        const std::optional<af::common::qod::PortsSpec>& ports);

    /**
     * @brief Validate duration
     * @param duration Duration to validate
     * @param qos_profile QoS profile for context
     * @return Error message if invalid, empty if valid
     */
    std::optional<std::string> validate_duration(
        std::chrono::seconds duration,
        const std::string& qos_profile);

    /**
     * @brief Validate notification sink
     * @param sink Sink URL to validate
     * @param credential Sink credential to validate
     * @return Error message if invalid, empty if valid
     */
    std::optional<std::string> validate_sink(
        const std::optional<std::string>& sink,
        const std::optional<af::common::qod::SinkCredential>& credential);

    // === JSON Conversion Methods ===

    /**
     * @brief Parse device from JSON
     * @param json JSON object
     * @return Parsed device or nullopt
     */
    std::optional<af::common::qod::QodDevice> parse_device(const nlohmann::json& json);

    /**
     * @brief Parse application server from JSON
     * @param json JSON object
     * @return Parsed application server
     */
    af::common::qod::ApplicationServer parse_application_server(const nlohmann::json& json);

    /**
     * @brief Parse port specification from JSON
     * @param json JSON object
     * @return Parsed port specification
     */
    std::optional<af::common::qod::PortsSpec> parse_ports(const nlohmann::json& json);

    /**
     * @brief Parse sink credential from JSON
     * @param json JSON object
     * @return Parsed sink credential
     */
    std::optional<af::common::qod::SinkCredential> parse_sink_credential(const nlohmann::json& json);

    /**
     * @brief Convert session to JSON response
     * @param session QoD session
     * @param include_device Whether to include device in response
     * @return JSON representation
     */
    nlohmann::json session_to_json(
        const af::common::qod::QodSession& session,
        bool include_device = false);

    /**
     * @brief Convert device to JSON response
     * @param device Device
     * @return JSON representation
     */
    nlohmann::json device_to_json(const af::common::qod::QodDevice& device);

    // === Error Response Methods ===

    /**
     * @brief Create error response
     * @param status HTTP status code
     * @param code Error code
     * @param message Error message
     * @param correlation_id Correlation ID for response
     * @return Error response message
     */
    af::communication::MessagePtr create_error_response(
        int status,
        const std::string& code,
        const std::string& message,
        const std::string& correlation_id = "");

    /**
     * @brief Create success response
     * @param data Response data
     * @param status HTTP status code
     * @param correlation_id Correlation ID for response
     * @return Success response message
     */
    af::communication::MessagePtr create_success_response(
        const nlohmann::json& data,
        int status = 200,
        const std::string& correlation_id = "");

    /**
     * @brief Extract request context from message
     * @param message Incoming message
     * @return Request context
     */
    QodRequestContext extract_context(const af::communication::MessagePtr& message);

    /**
     * @brief Extract session ID from message path
     * @param message Incoming message
     * @return Session ID or empty string
     */
    std::string extract_session_id(const af::communication::MessagePtr& message);

    // === Member Variables ===

    af::core::AfOrchestrator* orchestrator_;
    std::shared_ptr<QodSessionManager> session_manager_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace qod
} // namespace af