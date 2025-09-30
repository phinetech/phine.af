/**
 * @file qod_pcf_handler.cpp
 * @brief Implementation of the QoD PCF adapter
 */

#include "qod_pcf_handler.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <random>
#include <sstream>
#include <iomanip>

namespace af {
namespace southbound {

QodPcfHandler::QodPcfHandler(std::shared_ptr<UeStateManager> ue_state_manager)
    : ue_state_manager_(ue_state_manager) {
    
    // Setup logger
    initializeLogger(spdlog::level::debug);
    
    logger_->info("QoD PCF Adapter created");
    
    // Initialize default QoS profile mappings
    initialize_default_mappings();
}

QodPcfHandler::~QodPcfHandler() {
    // Clean up resources
}

void QodPcfHandler::initialize() {
    logger_->info("QoD PCF Adapter initialized");
    
    // Initialize PCF client
    pcf_client_->initialize();
    
    // Initialize PCC rule manager
    pcc_rule_manager_->initialize();
    
    // Initialize communication with AF Core
    initialize_communication();
    
    // Register message handlers
    register_handlers();
    
    logger_->info("PCF Handler initialization complete");
}

// Register handlers with communication service
// TODO: Fix me
void QodPcfHandler::register_handlers() {
    auto comm_services = orchestrator_->get_communication_services();
    auto pcf_comm_it = comm_services.find("pcf");

    // Register PCF response handlers for QoD
    request_router_->register_handler("pcf_qod_session_created",
        [this](const af::communication::MessagePtr& msg) {
            auto [success, pcf_session_id] = qod_pcf_adapter_->handle_pcf_create_response(msg);
            
            // Notify session manager of PCF response
            qod_session_manager_->handle_pcf_session_response(
                msg->correlation_id, pcf_session_id, success,
                success ? "" : "PCF session creation failed");
            
            // Return empty response (already handled internally)
            return std::make_shared<af::communication::Message>();
        });
    
    request_router_->register_handler("pcf_qod_session_updated",
        [this](const af::communication::MessagePtr& msg) {
            bool success = qod_pcf_adapter_->handle_pcf_update_response(msg);
            
            // Log the result
            if (success) {
                logger_->info("PCF session updated successfully");
            } else {
                logger_->error("PCF session update failed");
            }
            
            return std::make_shared<af::communication::Message>();
        });
    
    request_router_->register_handler("pcf_qod_session_deleted",
        [this](const af::communication::MessagePtr& msg) {
            bool success = qod_pcf_adapter_->handle_pcf_delete_response(msg);
            
            if (success) {
                logger_->info("PCF session deleted successfully");
            } else {
                logger_->error("PCF session deletion failed");
            }
            
            return std::make_shared<af::communication::Message>();
        });
    
    request_router_->register_handler("pcf_qod_notification",
        [this](const af::communication::MessagePtr& msg) {
            auto [qod_session_id, reason] = qod_pcf_adapter_->handle_pcf_notification(msg);
            
            if (qod_session_id) {
                // Get PCF session ID from adapter
                auto pcf_info = qod_pcf_adapter_->get_pcf_session_info(*qod_session_id);
                if (pcf_info) {
                    qod_session_manager_->handle_pcf_session_terminated(
                        pcf_info->pcf_session_id,
                        reason.value_or("NETWORK_TERMINATED"));
                }
            }
            
            return std::make_shared<af::communication::Message>();
        });
    
    // Register QoD notification callback handler (for CloudEvents delivery results)
    request_router_->register_handler("qod_notification_result",
        [this](const af::communication::MessagePtr& msg) {
            // Handle notification delivery results
            try {
                std::string payload_str(msg->payload.begin(), msg->payload.end());
                auto result_json = nlohmann::json::parse(payload_str);
                
                bool success = result_json.value("success", false);
                std::string session_id = result_json.value("session_id", "");
                std::string error = result_json.value("error", "");
                
                if (!success) {
                    logger_->warn("QoD notification delivery failed for session {}: {}", 
                                 session_id, error);
                }
            } catch (const std::exception& e) {
                logger_->error("Error processing notification result: {}", e.what());
            }
            
            return std::make_shared<af::communication::Message>();
        });
}

void QodPcfHandler::initializeLogger(spdlog::level::level_enum log_level) {
    logger_ = spdlog::get("qod_pcf_adapter");
    
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("qod_pcf_adapter");
    }
    
    logger_->set_level(log_level);
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void QodPcfHandler::initialize_default_mappings() {
    // CAMARA standard profiles
    qos_profile_mappings_["QOS_E"] = {
        1,          // 5QI = 1 (Conversational Voice)
        20,         // Priority
        100,        // Packet Delay Budget (ms)
        0.001,      // Packet Error Rate (10^-3)
        std::nullopt, // Max Data Burst
        true,       // GBR
        64,         // Guaranteed UL (kbps)
        64,         // Guaranteed DL (kbps)
        128,        // Max UL (kbps)
        128         // Max DL (kbps)
    };
    
    qos_profile_mappings_["QOS_S"] = {
        2,          // 5QI = 2 (Conversational Video)
        40,         // Priority
        150,        // Packet Delay Budget (ms)
        0.001,      // Packet Error Rate
        std::nullopt,
        true,       // GBR
        384,        // Guaranteed UL (kbps)
        384,        // Guaranteed DL (kbps)
        512,        // Max UL (kbps)
        512         // Max DL (kbps)
    };
    
    qos_profile_mappings_["QOS_M"] = {
        3,          // 5QI = 3 (Real Time Gaming)
        30,         // Priority
        50,         // Packet Delay Budget (ms)
        0.001,      // Packet Error Rate
        std::nullopt,
        true,       // GBR
        512,        // Guaranteed UL (kbps)
        512,        // Guaranteed DL (kbps)
        1024,       // Max UL (kbps)
        1024        // Max DL (kbps)
    };
    
    qos_profile_mappings_["QOS_L"] = {
        4,          // 5QI = 4 (Non-Conversational Video)
        50,         // Priority
        300,        // Packet Delay Budget (ms)
        0.000001,   // Packet Error Rate (10^-6)
        std::nullopt,
        true,       // GBR
        256,        // Guaranteed UL (kbps)
        256,        // Guaranteed DL (kbps)
        512,        // Max UL (kbps)
        512         // Max DL (kbps)
    };
    
    // Custom profiles
    qos_profile_mappings_["voice"] = {
        1, 20, 100, 0.001, std::nullopt, true, 64, 64, 128, 128
    };
    
    qos_profile_mappings_["video"] = {
        2, 40, 150, 0.001, std::nullopt, true, 1024, 2048, 2048, 4096
    };
    
    qos_profile_mappings_["game"] = {
        3, 30, 50, 0.001, std::nullopt, true, 512, 512, 1024, 1024
    };
    
    qos_profile_mappings_["data"] = {
        9,          // 5QI = 9 (Default non-GBR)
        60,         // Priority
        300,        // Packet Delay Budget (ms)
        0.000001,   // Packet Error Rate
        std::nullopt,
        false,      // Non-GBR
        std::nullopt, std::nullopt, std::nullopt, std::nullopt
    };
}

// === PCF Session Management ===

std::optional<af::communication::MessagePtr> QodPcfHandler::create_pcf_session(
    const af::common::qod::QodSession& qod_session) {
    
    logger_->info("Creating PCF session for QoD session: {}", qod_session.session_id);
    
    // Get QoS profile mapping
    auto mapping_opt = get_qos_profile_mapping(qod_session.qos_profile);
    if (!mapping_opt) {
        logger_->error("No mapping found for QoS profile: {}", qod_session.qos_profile);
        return std::nullopt;
    }
    auto& mapping = *mapping_opt;
    
    try {
        // Build PCF application session request
        nlohmann::json pcf_request;
        
        // Application session context
        pcf_request["ascReqData"] = build_app_session_context(qod_session);
        
        // AF request data
        pcf_request["afReqData"] = build_af_request_data(qod_session);
        
        // Media components
        pcf_request["medComponents"] = build_media_components(qod_session, mapping);
        
        // Subscription for notifications
        pcf_request["evSubsc"] = build_subscription_info(qod_session.session_id);
        
        // AF Application ID
        pcf_request["afAppId"] = af_id_;
        
        // UE identification
        pcf_request["ueId"] = translate_device_to_ue_id(
            qod_session.device, qod_session.ue_supi);
        
        // Service URN (optional, for specific services)
        pcf_request["servUrn"] = "urn:x-3gpp-qod:" + qod_session.qos_profile;
        
        // Generate PCF session ID
        std::string pcf_session_id = generate_pcf_session_id();
        
        // Store mapping
        store_session_mapping(qod_session.session_id, pcf_session_id, 
                            PcfSessionState::CREATING);
        
        // Create message
        auto msg = std::make_shared<af::communication::Message>();
        msg->message_type = "pcf_create_app_session";
        msg->correlation_id = qod_session.session_id;
        
        // Add PCF session ID to metadata
        msg->metadata["x-correlator"] = qod_session.session_id;
        msg->metadata["pcf_session_id"] = pcf_session_id;
        msg->metadata["operation"] = "create";

        
        // Set payload
        std::string payload = pcf_request.dump();
        msg->payload.assign(payload.begin(), payload.end());
        
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
    if (!pcf_info_opt) {
        logger_->error("No PCF session found for QoD session: {}", qod_session.session_id);
        return std::nullopt;
    }
    auto& pcf_info = *pcf_info_opt;
    
    // Get QoS profile mapping
    auto mapping_opt = get_qos_profile_mapping(qod_session.qos_profile);
    if (!mapping_opt) {
        logger_->error("No mapping found for QoS profile: {}", qod_session.qos_profile);
        return std::nullopt;
    }
    auto& mapping = *mapping_opt;
    
    try {
        // Build PCF update request
        nlohmann::json pcf_request;
        
        pcf_request["appSessionId"] = pcf_info.pcf_session_id;
        
        // Updated media components (for duration extension)
        pcf_request["medComponents"] = build_media_components(qod_session, mapping);
        
        // Update session duration
        pcf_request["maxReqBwDl"] = mapping.max_downlink_rate;
        pcf_request["maxReqBwUl"] = mapping.max_uplink_rate;
        
        // Create message
        auto msg = std::make_shared<af::communication::Message>();
        msg->message_type = "pcf_update_app_session";
        msg->correlation_id = qod_session.session_id;
        
        // Add metadata
        msg->metadata["x-correlator"] = qod_session.session_id;
        msg->metadata["pcf_session_id"] = pcf_info.pcf_session_id;
        msg->metadata["operation"] = "update";
        
        // Set payload
        std::string payload = pcf_request.dump();
        msg->payload.assign(payload.begin(), payload.end());
        
        // Update state
        update_session_state(pcf_info.pcf_session_id, PcfSessionState::UPDATING);
        
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
    if (!pcf_info_opt) {
        logger_->warn("No PCF session found for QoD session: {}", qod_session.session_id);
        return std::nullopt;
    }
    auto& pcf_info = *pcf_info_opt;
    
    try {
        // Build PCF delete request
        nlohmann::json pcf_request;
        pcf_request["appSessionId"] = pcf_info.pcf_session_id;
        
        // Create message
        auto msg = std::make_shared<af::communication::Message>();
        msg->message_type = "pcf_delete_app_session";
        msg->correlation_id = qod_session.session_id;
        
        // Add metadata
        msg->metadata["x-correlator"] = qod_session.session_id;
        msg->metadata["pcf_session_id"] = pcf_info.pcf_session_id;
        msg->metadata["operation"] = "delete";
        
        // Set payload
        std::string payload = pcf_request.dump();
        msg->payload.assign(payload.begin(), payload.end());
        
        // Update state
        update_session_state(pcf_info.pcf_session_id, PcfSessionState::DELETING);
        
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
        
        if (event_type == "SESSION_TERMINATED" || event_type == "RESOURCES_RELEASED") {
            // Session terminated by network
            std::string termination_reason = notification_json.value("reason", "NETWORK_TERMINATED");
            
            // Update state
            update_session_state(pcf_session_id, PcfSessionState::TERMINATED);
            
            logger_->info("PCF session terminated for QoD session {}: {}", 
                         qod_session_id, termination_reason);
            
            return {qod_session_id, termination_reason};
        }
        else if (event_type == "QOS_NOT_GUARANTEED") {
            // QoS cannot be guaranteed
            logger_->warn("QoS not guaranteed for session: {}", qod_session_id);
            return {qod_session_id, "QOS_NOT_GUARANTEED"};
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

// === QoS Profile Mapping ===

std::optional<QosProfileMapping> QodPcfHandler::get_qos_profile_mapping(
    const std::string& qos_profile) {
    
    std::lock_guard<std::mutex> lock(mappings_mutex_);
    
    auto it = qos_profile_mappings_.find(qos_profile);
    if (it != qos_profile_mappings_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void QodPcfHandler::register_qos_profile(const std::string& profile_name,
                                         const QosProfileMapping& mapping) {
    std::lock_guard<std::mutex> lock(mappings_mutex_);
    qos_profile_mappings_[profile_name] = mapping;
    logger_->info("Registered QoS profile mapping: {}", profile_name);
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

nlohmann::json QodPcfHandler::build_app_session_context(const af::common::qod::QodSession& qod_session) {
    nlohmann::json context;
    
    // DNN if resolved from UE state
    if (qod_session.pdu_session_id && ue_state_manager_) {
        // Try to get DNN from UE state
        if (qod_session.ue_supi) {
            auto ue_state = ue_state_manager_->get_ue_state_by_supi(*qod_session.ue_supi);
            if (ue_state) {
                for (const auto& pdu : ue_state->pdu_sessions) {
                    if (pdu.pdu_session_id == *qod_session.pdu_session_id) {
                        context["dnn"] = pdu.dnn.value;
                        if (pdu.snssai) {
                            nlohmann::json snssai;
                            snssai["sst"] = pdu.snssai->sst;
                            if (pdu.snssai->sd) {
                                snssai["sd"] = *pdu.snssai->sd;
                            }
                            context["sliceInfo"]["sNssai"] = snssai;
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // AF App ID
    context["afAppId"] = af_id_;
    
    // AF Service ID (optional, using QoS profile as service identifier)
    context["afServId"] = "qod-" + qod_session.qos_profile;
    
    // Sponsor ID (optional, for sponsored connectivity)
    // context["sponId"] = "sponsor123";
    
    // AF routing requirement
    context["afRoutReq"]["routeToLocs"] = nlohmann::json::array();
    
    // Temporal validity (session duration)
    context["tempValidities"] = nlohmann::json::array();
    nlohmann::json validity;
    
    auto now = std::chrono::system_clock::now();
    auto end_time = now + qod_session.duration;
    
    // Format times as RFC3339
    auto format_time = [](const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t));
        return std::string(buffer);
    };
    
    validity["startTime"] = format_time(now);
    validity["stopTime"] = format_time(end_time);
    context["tempValidities"].push_back(validity);
    
    return context;
}

nlohmann::json QodPcfHandler::build_media_components(
    const af::common::qod::QodSession& qod_session,
    const QosProfileMapping& mapping) {
    
    nlohmann::json med_comps = nlohmann::json::array();
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
    
    // QoS requirements
    if (mapping.is_gbr) {
        // Guaranteed Bit Rate
        if (mapping.guaranteed_downlink_rate) {
            med_comp["marBwDl"] = std::to_string(*mapping.guaranteed_downlink_rate) + " Kbps";
            med_comp["mirBwDl"] = std::to_string(*mapping.guaranteed_downlink_rate) + " Kbps";
        }
        if (mapping.guaranteed_uplink_rate) {
            med_comp["marBwUl"] = std::to_string(*mapping.guaranteed_uplink_rate) + " Kbps";
            med_comp["mirBwUl"] = std::to_string(*mapping.guaranteed_uplink_rate) + " Kbps";
        }
    }
    
    // Max bit rates
    if (mapping.max_downlink_rate) {
        med_comp["marBwDl"] = std::to_string(*mapping.max_downlink_rate) + " Kbps";
    }
    if (mapping.max_uplink_rate) {
        med_comp["marBwUl"] = std::to_string(*mapping.max_uplink_rate) + " Kbps";
    }
    
    // 5QI
    med_comp["qosReference"] = mapping.fiveqi;
    
    // Priority
    if (mapping.priority_level) {
        med_comp["resPrio"] = *mapping.priority_level;
    }
    
    // Media sub-components (flow descriptions)
    med_comp["medSubComps"] = map_ports_to_media_subcomponents(
        qod_session.device_ports,
        qod_session.application_server_ports);
    
    // Add flow descriptions
    auto flow_descs = build_flow_descriptions(qod_session);
    if (!flow_descs.empty()) {
        med_comp["fDescs"] = flow_descs;
    }
    
    med_comps.push_back(med_comp);
    
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
    event_subscription["events"].push_back("SESSION_TERMINATED");
    event_subscription["events"].push_back("QOS_NOT_GUARANTEED");
    event_subscription["events"].push_back("RESOURCES_RELEASED");
    
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
    
    // Traffic routes (optional)
    af_req_data["traffRoutRequ"] = nlohmann::json::array();
    
    // Application detection (optional)
    // af_req_data["appDetectionInfo"] = ...;
    
    // Charging information (optional)
    // af_req_data["chargingInfo"] = ...;
    
    return af_req_data;
}

nlohmann::json QodPcfHandler::map_ports_to_media_subcomponents(
    const std::optional<af::common::qod::PortsSpec>& device_ports,
    const std::optional<af::common::qod::PortsSpec>& server_ports) {
    
    nlohmann::json med_sub_comps = nlohmann::json::array();
    
    int flow_number = 1;
    
    // Create sub-component for each port combination
    auto add_subcomp = [&](uint16_t src_port, uint16_t dst_port, const std::string& dir) {
        nlohmann::json sub_comp;
        sub_comp["flowNumber"] = flow_number++;
        sub_comp["flowDirection"] = dir;
        
        // Flow usage (e.g., RTCP, RTP)
        sub_comp["flowUsage"] = "GENERAL";
        
        med_sub_comps.push_back(sub_comp);
    };
    
    if (device_ports && server_ports) {
        // Specific port mappings
        for (const auto& dev_port : device_ports->ports) {
            for (const auto& srv_port : server_ports->ports) {
                add_subcomp(dev_port, srv_port, "BIDIRECTIONAL");
            }
        }
        
        // Port ranges
        for (const auto& dev_range : device_ports->ranges) {
            for (const auto& srv_range : server_ports->ranges) {
                // Add representative flow for range
                add_subcomp(dev_range.from, srv_range.from, "BIDIRECTIONAL");
            }
        }
    } else {
        // Default bidirectional flow
        add_subcomp(0, 0, "BIDIRECTIONAL");
    }
    
    return med_sub_comps;
}

std::string QodPcfHandler::generate_pcf_session_id() {
    // Generate unique PCF session ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex_chars = "0123456789abcdef";
    std::stringstream ss;
    
    ss << "pcf-app-";
    for (int i = 0; i < 16; ++i) {
        ss << hex_chars[dis(gen)];
    }
    
    return ss.str();
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

} // namespace southbound
} // namespace af