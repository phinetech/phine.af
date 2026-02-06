/**
 * @file qod_pcf_adapter.h
 * @brief Adapter for translating CAMARA QoD requests to PCF interactions
 *
 * This class bridges the CAMARA QualityOnDemand API with the 3GPP PCF
 * (Policy Control Function) by translating QoD sessions into PCF application
 * sessions and managing the lifecycle of these sessions.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <chrono>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "pcf_client_wrapper.h"
#include <models/qod/qod_session.h>
#include <message.h>

namespace af {
namespace southbound {

/**
 * @brief PCF session state
 */
enum class PcfSessionState {
    CREATING,    // PCF session creation in progress
    ACTIVE,      // PCF session is active
    UPDATING,    // PCF session update in progress
    DELETING,    // PCF session deletion in progress
    TERMINATED   // PCF session has been terminated
};

/**
 * @brief PCF session information
 */
struct PcfSessionInfo {
    std::string pcf_session_id;          // PCF application session ID
    std::string qod_session_id;          // Corresponding QoD session ID
    PcfSessionState state;                // Current PCF session state
    std::chrono::system_clock::time_point created_at;
    std::optional<std::string> pcf_notification_uri; // PCF notification endpoint
    nlohmann::json last_request;         // Last request sent to PCF
    nlohmann::json last_response;        // Last response from PCF
};

/**
 * @brief Adapter for QoD to PCF translation
 */
class QodPcfHandler {
public:
    /**
     * @brief Constructor
     * @param config_path Path to configuration file
     */
    explicit QodPcfHandler(const std::string& config_path);

    /**
     * @brief Destructor
     */
    ~QodPcfHandler();

    /**
     * @brief Initialize the adapter
     * @param orchestrator Pointer to the orchestrator
     */
    void initialize();

    // === QoD Handlers ===
    /**
     * @brief Receive MessagePtr and convert to required model
     * @param message Incoming request message
     * @return Response message
     */
    af::communication::MessagePtr handle_qod_create_pcf_session(
        const af::communication::MessagePtr& message);


    /**
     * @brief Receive MessagePtr and convert to required model
     * @param message Incoming request message
     * @return Response message
     */
    af::communication::MessagePtr handle_qod_update_pcf_session(
        const af::communication::MessagePtr& message);

    /**
     * @brief Receive MessagePtr and convert to required model
     * @param message Incoming request message
     * @return Response message
     */
    af::communication::MessagePtr handle_qod_delete_pcf_session(
        const af::communication::MessagePtr& message);


    // === PCF Session Management ===

    /**
     * @brief Create PCF application session for QoD session
     * @param qod_session QoD session to create PCF session for
     * @return PCF request message or nullopt on failure
     */
    std::optional<af::communication::MessagePtr> create_pcf_session(
        const af::common::qod::QodSession& qod_session);

    /**
     * @brief Update PCF application session
     * @param qod_session Updated QoD session
     * @return PCF request message or nullopt on failure
     */
    std::optional<af::communication::MessagePtr> update_pcf_session(
        const af::common::qod::QodSession& qod_session);

    /**
     * @brief Delete PCF application session
     * @param qod_session QoD session to delete PCF session for
     * @return PCF request message or nullopt on failure
     */
    std::optional<af::communication::MessagePtr> delete_pcf_session(
        const af::common::qod::QodSession& qod_session);

    // === PCF Response Handling ===

    /**
     * @brief Handle PCF create session response
     * @param message PCF response message
     * @return Processed result with PCF session ID and status
     */
    std::pair<bool, std::string> handle_pcf_create_response(
        const af::communication::MessagePtr& message);

    /**
     * @brief Handle PCF update session response
     * @param message PCF response message
     * @return Success status
     */
    bool handle_pcf_update_response(
        const af::communication::MessagePtr& message);

    /**
     * @brief Handle PCF delete session response
     * @param message PCF response message
     * @return Success status
     */
    bool handle_pcf_delete_response(
        const af::communication::MessagePtr& message);

    /**
     * @brief Handle PCF notification
     * @param message PCF notification message
     * @return QoD session ID and termination reason if applicable
     */
    std::pair<std::optional<std::string>, std::optional<std::string>>
        handle_pcf_notification(const af::communication::MessagePtr& message);

    // === Session Mapping ===

    /**
     * @brief Get PCF session info by QoD session ID
     * @param qod_session_id QoD session ID
     * @return PCF session info or nullopt
     */
    std::optional<PcfSessionInfo> get_pcf_session_info(
        const std::string& qod_session_id);

    /**
     * @brief Get QoD session ID by PCF session ID
     * @param pcf_session_id PCF session ID
     * @return QoD session ID or empty string
     */
    std::string get_qod_session_id(const std::string& pcf_session_id);

    /**
     * @brief Initialize logger
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // === Translation Methods ===

    /**
     * @brief Build PCF request for creating application session
     * @param qod_session QoD session
     * @return PCF request json or nullopt on failure
     */
    nlohmann::json build_pcf_create_request(
        const af::common::qod::QodSession& qod_session);

    /**
     * @brief Build PCF application session context
     * @param qod_session QoD session
     * @return JSON representation of app session context
     */
    nlohmann::json build_app_session_context(const af::common::qod::QodSession& qod_session);

    /**
     * @brief Build media components for PCF
     * @param qod_session QoD session
     * @param mapping QoS profile mapping
     * @return JSON array of media components
     */
    nlohmann::json build_media_components(
        const af::common::qod::QodSession& qod_session);

    /**
     * @brief Build flow descriptions for PCF
     * @param qod_session QoD session
     * @return JSON array of flow descriptions
     */
    nlohmann::json build_flow_descriptions(const af::common::qod::QodSession& qod_session);

    /**
     * @brief Build subscription info for PCF notifications
     * @param qod_session_id QoD session ID
     * @return JSON representation of subscription info
     */
    nlohmann::json build_subscription_info(const std::string& qod_session_id);

    /**
     * @brief Translate device to UE identification for PCF
     * @param device QoD device
     * @param ue_supi Resolved SUPI if available
     * @return JSON representation of UE identification
     */
    nlohmann::json translate_device_to_ue_id(
        const std::optional<af::common::qod::QodDevice>& device,
        const std::optional<Supi>& ue_supi);

    /**
     * @brief Build AF request data for PCF
     * @param qod_session QoD session
     * @return JSON representation of AF request data
     */
    nlohmann::json build_af_request_data(const af::common::qod::QodSession& qod_session);

    /**
     * @brief Map QoD ports to PCF media sub-components
     * @param device_ports Device port specification
     * @param server_ports Server port specification
     * @return JSON array of media sub-components
     */
    nlohmann::json map_ports_to_media_subcomponents(
        const std::optional<af::common::qod::PortsSpec>& device_ports,
        const std::optional<af::common::qod::PortsSpec>& server_ports);

    /**
     * @brief Store PCF session mapping
     * @param qod_session_id QoD session ID
     * @param pcf_session_id PCF session ID
     * @param state Initial state
     */
    void store_session_mapping(const std::string& qod_session_id,
                              const std::string& pcf_session_id,
                              PcfSessionState state);

    /**
     * @brief Update PCF session state
     * @param pcf_session_id PCF session ID
     * @param state New state
     */
    void update_session_state(const std::string& pcf_session_id,
                             PcfSessionState state);

    /**
     * @brief Remove PCF session mapping
     * @param qod_session_id QoD session ID
     */
    void remove_session_mapping(const std::string& qod_session_id);

    // === Parsing Methods ===

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

    // === Response helpers ===
    // Now using ServiceHandlerHelpers::create_success_response from common

    /**
     * @brief Load configuration from file
     * @param path Path to configuration file
     */
    void load_config(const std::string& path);

    // === Member Variables ===

    // Configuration
    std::string config_path_;
    std::string pcf_base_url_;
    bool use_tls_;
    std::string api_version_;

    std::shared_ptr<PcfClientWrapper> pcf_client_;



    // Session mappings
    std::unordered_map<std::string, PcfSessionInfo> qod_to_pcf_sessions_; // QoD ID -> PCF info
    std::unordered_map<std::string, std::string> pcf_to_qod_sessions_;    // PCF ID -> QoD ID
    mutable std::mutex sessions_mutex_;

    // AF identification for PCF
    std::string af_id_{"af_qod_service"};
    std::string af_notification_uri_{"http://af-core:50051/qod/notifications"};

    // Logger
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace southbound
} // namespace af