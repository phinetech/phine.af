/**
 * @file qod_session_manager.cpp
 * @brief Implementation of the QoD session manager
 */

#include "qod/qod_session_manager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <random>
#include <sstream>
#include <iomanip>
#include "af_orchestrator.h"

namespace af {
namespace qod {

QodSessionManager::QodSessionManager(
    std::shared_ptr<UeStateManager> ue_state_manager,
    const QodSessionConfig& config)
    : ue_state_manager_(ue_state_manager), config_(config) {
    
    // Setup logger
    initializeLogger(spdlog::level::debug);
    
    logger_->info("QoD Session Manager created");
    
    // Initialize default max durations for standard profiles if not configured
    if (config_.qos_profile_max_durations.empty()) {
        config_.qos_profile_max_durations = {
            {"QOS_E", std::chrono::seconds(3600)},    // 1 hour for enhanced
            {"QOS_S", std::chrono::seconds(7200)},    // 2 hours for standard
            {"QOS_M", std::chrono::seconds(14400)},   // 4 hours for medium
            {"QOS_L", std::chrono::seconds(28800)},   // 8 hours for low latency
            {"voice", std::chrono::seconds(7200)},    // 2 hours for voice
            {"video", std::chrono::seconds(14400)},   // 4 hours for video
            {"game", std::chrono::seconds(28800)},    // 8 hours for gaming
            {"data", std::chrono::seconds(86400)}     // 24 hours for data
        };
    }
}

QodSessionManager::~QodSessionManager() {
    stop();
}

void QodSessionManager::initialize(af::core::AfOrchestrator* orchestrator) {
    orchestrator_ = orchestrator;
    logger_->info("QoD Session Manager initialized");
}

void QodSessionManager::start() {
    logger_->info("Starting QoD Session Manager");
    
    // Start cleanup thread
    cleanup_running_ = true;
    cleanup_thread_ = std::thread(&QodSessionManager::session_cleanup_thread, this);
    
    logger_->info("QoD Session Manager started");
}

void QodSessionManager::stop() {
    logger_->info("Stopping QoD Session Manager");
    
    // Stop cleanup thread
    cleanup_running_ = false;
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    
    // Clean up all sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [id, session] : sessions_) {
            if (session.qos_status == QosStatus::AVAILABLE) {
                // Try to remove from PCF
                remove_session_from_pcf(session);
            }
        }
        sessions_.clear();
        pcf_to_qod_session_.clear();
    }
    
    logger_->info("QoD Session Manager stopped");
}

void QodSessionManager::initializeLogger(spdlog::level::level_enum log_level) {
    logger_ = spdlog::get("qod_session_mgr");
    
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("qod_session_mgr");
    }
    
    logger_->set_level(log_level);
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

// === Session Management Operations ===

std::optional<QodSession> QodSessionManager::create_session(
    const CreateSessionRequest& request) {
    
    logger_->info("Creating QoD session for profile: {}", request.qos_profile);
    
    // Validate QoS profile
    if (!validate_qos_profile(request.qos_profile)) {
        logger_->error("Invalid or unavailable QoS profile: {}", request.qos_profile);
        return std::nullopt;
    }
    
    // Validate duration
    auto max_duration = get_max_duration_for_profile(request.qos_profile);
    if (request.duration > max_duration || request.duration < config_.min_session_duration) {
        logger_->error("Duration {} out of range for profile {}", 
                      request.duration.count(), request.qos_profile);
        return std::nullopt;
    }
    
    // Resolve device if provided
    std::optional<Supi> resolved_supi;
    std::optional<std::string> pdu_session_id;
    if (request.device) {
        auto [supi, pdu_id] = resolve_device(*request.device);
        resolved_supi = supi;
        pdu_session_id = pdu_id;
        
        if (!resolved_supi) {
            logger_->warn("Could not resolve device to SUPI");
            // Continue anyway - PCF might be able to resolve
        }
    }
    
    // Check for conflicting sessions
    if (request.device) {
        auto conflict = check_session_conflict(*request.device, request.application_server);
        if (conflict) {
            logger_->error("Conflicting session exists: {}", *conflict);
            return std::nullopt;
        }
    }
    
    // Create new session
    QodSession session;
    session.session_id = generate_session_id();
    session.api_consumer_id = request.api_consumer_id;
    session.device = request.device;
    session.application_server = request.application_server;
    session.device_ports = request.device_ports;
    session.application_server_ports = request.application_server_ports;
    session.qos_profile = request.qos_profile;
    session.duration = request.duration;
    session.sink = request.sink;
    session.sink_credential = request.sink_credential;
    session.created_at = std::chrono::system_clock::now();
    session.qos_status = QosStatus::REQUESTED;
    session.ue_supi = resolved_supi;
    session.pdu_session_id = pdu_session_id;
    
    // Determine which device identifier to return in response
    if (request.device && request.api_consumer_id.find("2-legged") != std::string::npos) {
        // For 2-legged tokens, return one device identifier
        // Priority: phone_number > ipv4_address > ipv6_address
        QodDevice response_device;
        if (request.device->phone_number) {
            response_device.phone_number = request.device->phone_number;
        } else if (request.device->ipv4_address) {
            response_device.ipv4_address = request.device->ipv4_address;
        } else if (request.device->ipv6_address) {
            response_device.ipv6_address = request.device->ipv6_address;
        }
        if (!response_device.is_empty()) {
            session.device_response = response_device;
        }
    }
    
    // Store session
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[session.session_id] = session;
    }
    
    // Apply to PCF
    bool pcf_result = apply_session_to_pcf(session);
    
    if (!pcf_result) {
        // PCF application failed immediately
        logger_->error("Failed to apply session to PCF");
        
        // Update session status
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_[session.session_id].qos_status = QosStatus::UNAVAILABLE;
            sessions_[session.session_id].status_info = StatusInfo::NETWORK_TERMINATED;
        }
        
        // Send notification if configured
        if (request.sink && config_.enable_notifications) {
            send_status_change_notification(
                sessions_[session.session_id],
                QosStatus::REQUESTED,
                StatusInfo::NETWORK_TERMINATED);
        }
        
        return sessions_[session.session_id];
    }
    
    logger_->info("QoD session created with ID: {}", session.session_id);
    
    // Return the session in REQUESTED state
    // PCF will asynchronously confirm when it's AVAILABLE
    return session;
}

std::optional<QodSession> QodSessionManager::get_session(
    const std::string& session_id,
    const std::string& api_consumer_id) {
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        logger_->debug("Session not found: {}", session_id);
        return std::nullopt;
    }
    
    // Check authorization
    if (it->second.api_consumer_id != api_consumer_id) {
        logger_->warn("Unauthorized access to session {} by consumer {}", 
                     session_id, api_consumer_id);
        return std::nullopt;
    }
    
    return it->second;
}

bool QodSessionManager::delete_session(
    const std::string& session_id,
    const std::string& api_consumer_id) {
    
    logger_->info("Deleting session: {}", session_id);
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        logger_->debug("Session not found: {}", session_id);
        return false;
    }
    
    // Check authorization
    if (it->second.api_consumer_id != api_consumer_id) {
        logger_->warn("Unauthorized deletion attempt for session {} by consumer {}", 
                     session_id, api_consumer_id);
        return false;
    }
    
    QodSession& session = it->second;
    QosStatus old_status = session.qos_status;
    
    // Remove from PCF if active
    if (session.qos_status == QosStatus::AVAILABLE) {
        remove_session_from_pcf(session);
    }
    
    // Update status
    session.qos_status = QosStatus::UNAVAILABLE;
    session.status_info = StatusInfo::DELETE_REQUESTED;
    session.expires_at = std::chrono::system_clock::now();
    
    // Send notification if configured
    if (session.sink && config_.enable_notifications && old_status == QosStatus::AVAILABLE) {
        send_status_change_notification(session, old_status, StatusInfo::DELETE_REQUESTED);
    }
    
    // Mark for cleanup (will be removed after TTL)
    // This allows polling clients to see the UNAVAILABLE status
    
    logger_->info("Session {} marked as deleted", session_id);
    return true;
}

std::optional<QodSession> QodSessionManager::extend_session_duration(
    const ExtendSessionDurationRequest& request,
    const std::string& api_consumer_id) {
    
    logger_->info("Extending session {} by {} seconds", 
                 request.session_id, request.requested_additional_duration.count());
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(request.session_id);
    if (it == sessions_.end()) {
        logger_->debug("Session not found: {}", request.session_id);
        return std::nullopt;
    }
    
    // Check authorization
    if (it->second.api_consumer_id != api_consumer_id) {
        logger_->warn("Unauthorized extension attempt for session {} by consumer {}", 
                     request.session_id, api_consumer_id);
        return std::nullopt;
    }
    
    QodSession& session = it->second;
    
    // Can only extend AVAILABLE sessions
    if (session.qos_status != QosStatus::AVAILABLE) {
        logger_->error("Cannot extend session {} in status {}", 
                      request.session_id, QodTypeUtils::qos_status_to_string(session.qos_status));
        return std::nullopt;
    }
    
    // Calculate new duration
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - *session.started_at);
    auto new_total_duration = elapsed + request.requested_additional_duration;
    
    // Check against maximum duration for profile
    auto max_duration = get_max_duration_for_profile(session.qos_profile);
    if (new_total_duration > max_duration) {
        new_total_duration = max_duration;
        logger_->info("Capping extended duration to maximum: {} seconds", max_duration.count());
    }
    
    // Update PCF session
    if (!update_session_in_pcf(session, new_total_duration)) {
        logger_->error("Failed to extend session in PCF");
        return std::nullopt;
    }
    
    // Update session
    session.duration = new_total_duration;
    session.expires_at = *session.started_at + new_total_duration;
    
    logger_->info("Session {} extended to {} seconds total duration", 
                 request.session_id, new_total_duration.count());
    
    return session;
}

std::vector<QodSession> QodSessionManager::retrieve_sessions_by_device(
    const RetrieveSessionsRequest& request) {
    
    logger_->debug("Retrieving sessions for device");
    
    std::vector<QodSession> result;
    
    // Resolve device if provided
    std::optional<Supi> resolved_supi;
    if (request.device) {
        auto [supi, pdu_id] = resolve_device(*request.device);
        resolved_supi = supi;
    }
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (const auto& [id, session] : sessions_) {
        // Check API consumer authorization
        if (session.api_consumer_id != request.api_consumer_id) {
            continue;
        }
        
        // Match device
        bool matches = false;
        
        if (!request.device) {
            // No device specified, return all for this consumer
            matches = true;
        } else if (resolved_supi && session.ue_supi) {
            // Match by SUPI
            matches = (*resolved_supi == *session.ue_supi);
        } else if (request.device && session.device) {
            // Match by device identifiers
            if (request.device->phone_number && session.device->phone_number) {
                matches = (*request.device->phone_number == *session.device->phone_number);
            } else if (request.device->ipv4_address && session.device->ipv4_address) {
                matches = (*request.device->ipv4_address == *session.device->ipv4_address);
            } else if (request.device->ipv6_address && session.device->ipv6_address) {
                matches = (*request.device->ipv6_address == *session.device->ipv6_address);
            }
        }
        
        if (matches) {
            result.push_back(session);
        }
    }
    
    logger_->debug("Found {} sessions for device", result.size());
    return result;
}

// === PCF Integration ===

void QodSessionManager::handle_pcf_session_response(
    const std::string& session_id,
    const std::string& pcf_session_id,
    bool success,
    const std::string& error_message) {
    
    logger_->info("PCF response for session {}: success={}, pcf_id={}", 
                 session_id, success, pcf_session_id);
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        logger_->error("Session not found: {}", session_id);
        return;
    }
    
    QodSession& session = it->second;
    QosStatus old_status = session.qos_status;
    
    if (success) {
        // Session is now available
        session.qos_status = QosStatus::AVAILABLE;
        session.pcf_session_id = pcf_session_id;
        session.started_at = std::chrono::system_clock::now();
        session.expires_at = *session.started_at + session.duration;
        
        // Map PCF ID to QoD ID
        pcf_to_qod_session_[pcf_session_id] = session_id;
        
        // Send notification
        if (session.sink && config_.enable_notifications) {
            send_status_change_notification(session, old_status, std::nullopt);
        }
    } else {
        // Session failed
        session.qos_status = QosStatus::UNAVAILABLE;
        session.status_info = StatusInfo::NETWORK_TERMINATED;
        session.error_message = error_message;
        
        // Send notification
        if (session.sink && config_.enable_notifications) {
            send_status_change_notification(session, old_status, StatusInfo::NETWORK_TERMINATED);
        }
    }
}

void QodSessionManager::handle_pcf_session_terminated(
    const std::string& pcf_session_id,
    const std::string& reason) {
    
    logger_->info("PCF session terminated: pcf_id={}, reason={}", pcf_session_id, reason);
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Find QoD session by PCF ID
    auto pcf_it = pcf_to_qod_session_.find(pcf_session_id);
    if (pcf_it == pcf_to_qod_session_.end()) {
        logger_->warn("No QoD session found for PCF session: {}", pcf_session_id);
        return;
    }
    
    auto qod_it = sessions_.find(pcf_it->second);
    if (qod_it == sessions_.end()) {
        logger_->error("QoD session not found: {}", pcf_it->second);
        return;
    }
    
    QodSession& session = qod_it->second;
    
    if (session.qos_status == QosStatus::AVAILABLE) {
        QosStatus old_status = session.qos_status;
        session.qos_status = QosStatus::UNAVAILABLE;
        session.status_info = StatusInfo::NETWORK_TERMINATED;
        session.expires_at = std::chrono::system_clock::now();
        
        // Send notification
        if (session.sink && config_.enable_notifications) {
            send_status_change_notification(session, old_status, StatusInfo::NETWORK_TERMINATED);
        }
    }
    
    // Remove PCF mapping
    pcf_to_qod_session_.erase(pcf_session_id);
}

// === Private Methods ===

std::pair<std::optional<Supi>, std::optional<std::string>> 
QodSessionManager::resolve_device(const QodDevice& device) {
    
    if (!ue_state_manager_) {
        return {std::nullopt, std::nullopt};
    }
    
    std::optional<AfUeSubscriptionState> ue_state;
    
    // Try to resolve by different identifiers
    if (device.phone_number) {
        // Convert phone number to GPSI format
        Gpsi gpsi{*device.phone_number};
        ue_state = ue_state_manager_->get_ue_state_by_gpsi(gpsi);
    }
    
    if (!ue_state && device.ipv4_address) {
        if (device.ipv4_address->public_address.value.empty() == false) {
            Ipv4Addr addr{device.ipv4_address->public_address.value};
            ue_state = ue_state_manager_->get_ue_state_by_ipv4(addr);
        }
        if (!ue_state && device.ipv4_address->private_address) {
            Ipv4Addr addr{device.ipv4_address->private_address->value};
            ue_state = ue_state_manager_->get_ue_state_by_ipv4(addr);
        }
    }
    
    if (!ue_state && device.ipv6_address) {
        Ipv6Addr addr{device.ipv6_address->value};
        ue_state = ue_state_manager_->get_ue_state_by_ipv6_addr(addr);
    }
    
    if (ue_state) {
        // Get the first active PDU session if available
        std::optional<std::string> pdu_session_id;
        if (!ue_state->pdu_sessions.empty()) {
            for (const auto& pdu_session : ue_state->pdu_sessions) {
                if (pdu_session.status == "ACTIVE") {
                    pdu_session_id = pdu_session.pdu_session_id;
                    break;
                }
            }
        }
        return {ue_state->supi, pdu_session_id};
    }
    
    return {std::nullopt, std::nullopt};
}

bool QodSessionManager::validate_qos_profile(const std::string& profile) {
    // Check if profile is supported
    if (supported_profiles_.find(profile) == supported_profiles_.end()) {
        return false;
    }
    
    // TODO: Check if profile is currently available (not INACTIVE or DEPRECATED)
    // This would involve checking with a QoS Profile service or configuration
    
    return true;
}

std::optional<std::string> QodSessionManager::check_session_conflict(
    const QodDevice& device,
    const ApplicationServer& app_server) {
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (const auto& [id, session] : sessions_) {
        // Only check REQUESTED or AVAILABLE sessions
        if (session.qos_status == QosStatus::UNAVAILABLE) {
            continue;
        }
        
        // Check if same device
        bool same_device = false;
        if (device.phone_number && session.device && session.device->phone_number) {
            same_device = (*device.phone_number == *session.device->phone_number);
        } else if (device.ipv4_address && session.device && session.device->ipv4_address) {
            same_device = (*device.ipv4_address == *session.device->ipv4_address);
        } else if (device.ipv6_address && session.device && session.device->ipv6_address) {
            same_device = (*device.ipv6_address == *session.device->ipv6_address);
        }
        
        if (same_device) {
            // Check if same application server
            bool same_server = false;
            if (app_server.ipv4_address && session.application_server.ipv4_address) {
                same_server = (*app_server.ipv4_address == *session.application_server.ipv4_address);
            } else if (app_server.ipv6_address && session.application_server.ipv6_address) {
                same_server = (*app_server.ipv6_address == *session.application_server.ipv6_address);
            }
            
            if (same_server) {
                return id;  // Conflict found
            }
        }
    }
    
    return std::nullopt;
}

bool QodSessionManager::apply_session_to_pcf(QodSession& session) {
    logger_->info("Applying QoD session {} to PCF", session.session_id);
    
    if (!orchestrator_) {
        logger_->error("Orchestrator not initialized");
        return false;
    }

    std::shared_ptr<af::communication::CommunicationService> pcf_comm;

    try {
        logger_->debug("Retrieving PCF communication service");
        // Get PCF communication service
        auto& comm_services = orchestrator_->get_communication_services();
        logger_->debug("Available communication services:");

        // Check if comm_services is valid and not a null pointer
        
        if (comm_services.empty()) {
            logger_->error("No communication services available from orchestrator.");
            return false;
        }
        logger_->debug("Communication services count: {}", comm_services.size());

        if (comm_services.empty() && comm_services.find("pcf") == comm_services.end()) {
            logger_->error("Orchestrator returned an empty or invalid communication services map.");
            return false;
        }

        auto pcf_comm_it = comm_services.find("pcf");
        logger_->debug("PCF communication service found");

        if (pcf_comm_it == comm_services.end() || !pcf_comm_it->second) {
            logger_->error("PCF communication service not available");
            return false; // Log error and return, do not exit program
        }
        logger_->debug("PCF communication service is available");

        pcf_comm = pcf_comm_it->second;
    }
    catch (const std::exception& e) {
        logger_->error("Error accessing PCF communication service: {}", e.what());
        return false;
    }
    

    try {
        // Build PCF request
        auto pcf_request = build_pcf_request(session);
        
        // Create message for PCF
        auto msg = std::make_shared<af::communication::Message>();
        msg->message_type = "pcf_create_qod_session";
        msg->correlation_id = session.session_id;
        
        // Set payload
        std::string payload = pcf_request.dump();
        msg->payload.assign(payload.begin(), payload.end());
        
        // Store PCF transaction ID for tracking
        session.pcf_transaction_id = session.session_id;
        
        logger_->debug("Sending QoD session to PCF: {}", payload);
        
        // Send async request to PCF
        // The response will come back via handle_pcf_session_response
        // TODO: Use actual PCF address from config
        auto response = pcf_comm->send_request("192.168.70.140:50055", msg);
        
        if (!response) {
            logger_->error("Failed to send request to PCF");
            return false;
        }
        
        // Process immediate response if synchronous
        if (response->message_type == "pcf_qod_session_created") {
            std::string response_str(response->payload.begin(), response->payload.end());
            auto response_json = nlohmann::json::parse(response_str);
            
            std::string pcf_session_id = response_json.value("app_session_id", "");
            handle_pcf_session_response(session.session_id, pcf_session_id, true);
        } else if (response->message_type == "pcf_error") {
            std::string error_str(response->payload.begin(), response->payload.end());
            auto error_json = nlohmann::json::parse(error_str);
            
            std::string error_message = error_json.value("message", "Unknown error");
            handle_pcf_session_response(session.session_id, "", false, error_message);
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logger_->error("Error applying session to PCF: {}", e.what());
        return false;
    }
}

bool QodSessionManager::remove_session_from_pcf(const QodSession& session) {
    logger_->info("Removing QoD session {} from PCF", session.session_id);
    
    if (!session.pcf_session_id) {
        logger_->debug("No PCF session ID, nothing to remove");
        return true;
    }
    
    if (!orchestrator_) {
        logger_->error("Orchestrator not initialized");
        return false;
    }
    
    auto& comm_services = orchestrator_->get_communication_services();
    auto pcf_comm_it = comm_services.find("pcf");
    
    if (pcf_comm_it == comm_services.end() || !pcf_comm_it->second) {
        logger_->error("PCF communication service not available");
        return false;
    }
    
    auto& pcf_comm = pcf_comm_it->second;
    
    try {
        // Create delete request
        nlohmann::json delete_request = {
            {"app_session_id", *session.pcf_session_id}
        };
        
        // Create message for PCF
        auto msg = std::make_shared<af::communication::Message>();
        msg->message_type = "pcf_delete_qod_session";
        msg->correlation_id = session.session_id;
        
        std::string payload = delete_request.dump();
        msg->payload.assign(payload.begin(), payload.end());
        
        logger_->debug("Sending delete request to PCF for session: {}", *session.pcf_session_id);
        
        // TODO: Use actual PCF address from config
        auto response = pcf_comm->send_request("192.168.70.140:50055", msg);
        
        if (response && response->message_type == "pcf_qod_session_deleted") {
            logger_->info("QoD session removed from PCF successfully");
            return true;
        }
        
        return false;
    }
    catch (const std::exception& e) {
        logger_->error("Error removing session from PCF: {}", e.what());
        return false;
    }
}

bool QodSessionManager::update_session_in_pcf(QodSession& session, std::chrono::seconds new_duration) {
    logger_->info("Updating QoD session {} in PCF with new duration: {} seconds", 
                 session.session_id, new_duration.count());
    
    if (!session.pcf_session_id) {
        logger_->error("No PCF session ID to update");
        return false;
    }
    
    if (!orchestrator_) {
        logger_->error("Orchestrator not initialized");
        return false;
    }
    
    auto& comm_services = orchestrator_->get_communication_services();
    auto pcf_comm_it = comm_services.find("pcf");
    
    if (pcf_comm_it == comm_services.end() || !pcf_comm_it->second) {
        logger_->error("PCF communication service not available");
        return false;
    }
    
    auto& pcf_comm = pcf_comm_it->second;
    
    try {
        // Create update request
        nlohmann::json update_request = {
            {"app_session_id", *session.pcf_session_id},
            {"duration", new_duration.count()}
        };
        
        // Create message for PCF
        auto msg = std::make_shared<af::communication::Message>();
        msg->message_type = "pcf_update_qod_session";
        msg->correlation_id = session.session_id;
        
        std::string payload = update_request.dump();
        msg->payload.assign(payload.begin(), payload.end());
        
        logger_->debug("Sending update request to PCF: {}", payload);
        
        // TODO: Use actual PCF address from config
        auto response = pcf_comm->send_request("192.168.70.140:50055", msg);
        
        if (response && response->message_type == "pcf_qod_session_updated") {
            logger_->info("QoD session updated in PCF successfully");
            return true;
        }
        
        return false;
    }
    catch (const std::exception& e) {
        logger_->error("Error updating session in PCF: {}", e.what());
        return false;
    }
}

void QodSessionManager::send_status_change_notification(
    const QodSession& session,
    QosStatus old_status,
    std::optional<StatusInfo> status_info) {
    
    if (!session.sink || !notification_handler_) {
        return;
    }
    
    logger_->info("Sending status change notification for session {}: {} -> {}", 
                 session.session_id,
                 QodTypeUtils::qos_status_to_string(old_status),
                 QodTypeUtils::qos_status_to_string(session.qos_status));
    
    // Create CloudEvent
    auto event = QodEventBuilder::create_qos_status_changed_event(
        session.session_id,
        session.qos_status,
        status_info ? status_info : session.status_info,
        config_.api_base_url);
    
    // Deliver notification
    auto result = notification_handler_->deliver(
        event,
        *session.sink,
        session.sink_credential);
    
    if (!result.success) {
        logger_->error("Failed to deliver notification: {}", result.error_message);
    }
}

void QodSessionManager::cleanup_expired_sessions() {
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> to_remove;
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        for (auto& [id, session] : sessions_) {
            bool should_remove = false;
            
            // Check if session has expired
            if (session.qos_status == QosStatus::AVAILABLE && session.expires_at) {
                if (now >= *session.expires_at) {
                    // Session duration expired
                    logger_->info("Session {} duration expired", id);
                    
                    QosStatus old_status = session.qos_status;
                    session.qos_status = QosStatus::UNAVAILABLE;
                    session.status_info = StatusInfo::DURATION_EXPIRED;
                    
                    // Remove from PCF
                    remove_session_from_pcf(session);
                    
                    // Send notification
                    if (session.sink && config_.enable_notifications) {
                        send_status_change_notification(
                            session, old_status, StatusInfo::DURATION_EXPIRED);
                    }
                }
            }
            
            // Check if unavailable session should be cleaned up
            if (session.qos_status == QosStatus::UNAVAILABLE) {
                auto unavailable_duration = now - session.created_at;
                if (session.expires_at) {
                    unavailable_duration = now - *session.expires_at;
                }
                
                if (unavailable_duration >= config_.unavailable_session_ttl) {
                    should_remove = true;
                    logger_->debug("Removing expired unavailable session: {}", id);
                }
            }
            
            if (should_remove) {
                to_remove.push_back(id);
            }
        }
        
        // Remove expired sessions
        for (const auto& id : to_remove) {
            auto it = sessions_.find(id);
            if (it != sessions_.end()) {
                // Clean up PCF mapping if exists
                if (it->second.pcf_session_id) {
                    pcf_to_qod_session_.erase(*it->second.pcf_session_id);
                }
                sessions_.erase(it);
            }
        }
    }
    
    if (!to_remove.empty()) {
        logger_->info("Cleaned up {} expired sessions", to_remove.size());
    }
}

void QodSessionManager::session_cleanup_thread() {
    logger_->info("Session cleanup thread started");
    
    while (cleanup_running_) {
        std::this_thread::sleep_for(config_.session_cleanup_interval);
        
        if (!cleanup_running_) {
            break;
        }
        
        cleanup_expired_sessions();
    }
    
    logger_->info("Session cleanup thread stopped");
}

std::string QodSessionManager::generate_session_id() {
    // Generate UUID v4
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex_chars = "0123456789abcdef";
    std::stringstream ss;
    
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            ss << '-';
        } else if (i == 14) {
            ss << '4';  // Version 4
        } else {
            ss << hex_chars[dis(gen)];
        }
    }
    
    return ss.str();
}

nlohmann::json QodSessionManager::build_pcf_request(const QodSession& session) {
    nlohmann::json request;
    
    // Basic session information
    request["qod_session_id"] = session.session_id;
    request["qos_profile"] = session.qos_profile;
    request["duration"] = session.duration.count();
    
    // Device information
    if (session.device) {
        request["device"] = convert_device_to_pcf(*session.device, session.ue_supi);
    } else if (session.ue_supi) {
        request["supi"] = session.ue_supi->value;
    }
    
    // Application server
    if (session.application_server.ipv4_address) {
        request["app_server_ipv4"] = *session.application_server.ipv4_address;
    }
    if (session.application_server.ipv6_address) {
        request["app_server_ipv6"] = *session.application_server.ipv6_address;
    }
    
    // Flow filters
    request["flow_info"] = build_flow_filters(session);
    
    // Map QoS profile to 5QI and other parameters
    // This mapping would be configured based on operator policies
    int fiveqi = 9;  // Default
    if (session.qos_profile == "QOS_E" || session.qos_profile == "voice") {
        fiveqi = 1;  // Conversational voice
    } else if (session.qos_profile == "QOS_S" || session.qos_profile == "video") {
        fiveqi = 2;  // Conversational video
    } else if (session.qos_profile == "QOS_M" || session.qos_profile == "game") {
        fiveqi = 3;  // Real-time gaming
    } else if (session.qos_profile == "QOS_L") {
        fiveqi = 4;  // Non-conversational video
    }
    
    request["5qi"] = fiveqi;
    
    return request;
}

nlohmann::json QodSessionManager::convert_device_to_pcf(
    const QodDevice& device,
    const std::optional<Supi>& supi) {
    
    nlohmann::json pcf_device;
    
    if (supi) {
        pcf_device["supi"] = supi->value;
    }
    
    if (device.phone_number) {
        pcf_device["gpsi"] = *device.phone_number;
    }
    
    if (device.ipv4_address) {
        pcf_device["ipv4"] = device.ipv4_address->public_address.value;
        if (device.ipv4_address->private_address) {
            pcf_device["ipv4_private"] = device.ipv4_address->private_address->value;
        }
        if (device.ipv4_address->public_port) {
            pcf_device["port"] = *device.ipv4_address->public_port;
        }
    }
    
    if (device.ipv6_address) {
        pcf_device["ipv6"] = device.ipv6_address->value;
    }
    
    return pcf_device;
}

nlohmann::json QodSessionManager::build_flow_filters(const QodSession& session) {
    nlohmann::json flow_info = nlohmann::json::array();
    
    nlohmann::json flow;
    flow["flow_id"] = 1;
    flow["flow_direction"] = "BIDIRECTIONAL";
    
    // Build flow description based on ports
    std::vector<std::string> flow_descs;
    
    // Basic flow: permit all between device and app server
    std::string base_flow = "permit out ";
    
    // Add protocol (assume TCP/UDP for now)
    base_flow += "17 from ";  // UDP
    
    // Source (device)
    if (session.device && session.device->ipv4_address) {
        base_flow += session.device->ipv4_address->public_address.value;
    } else {
        base_flow += "any";
    }
    
    // Source ports
    if (session.device_ports && !session.device_ports->ports.empty()) {
        base_flow += " " + std::to_string(session.device_ports->ports[0]);
    } else {
        base_flow += " to ";
    }
    
    // Destination (app server)
    if (session.application_server.ipv4_address) {
        base_flow += " " + *session.application_server.ipv4_address;
    } else if (session.application_server.ipv6_address) {
        base_flow += " " + *session.application_server.ipv6_address;
    } else {
        base_flow += " any";
    }
    
    // Destination ports
    if (session.application_server_ports && !session.application_server_ports->ports.empty()) {
        base_flow += " " + std::to_string(session.application_server_ports->ports[0]);
    }
    
    flow_descs.push_back(base_flow);
    flow["flow_descriptions"] = flow_descs;
    
    flow_info.push_back(flow);
    
    return flow_info;
}

std::chrono::seconds QodSessionManager::get_max_duration_for_profile(const std::string& profile) {
    auto it = config_.qos_profile_max_durations.find(profile);
    if (it != config_.qos_profile_max_durations.end()) {
        return it->second;
    }
    return config_.max_session_duration;  // Default max
}

// === Public utility methods ===

void QodSessionManager::set_notification_handler(
    std::shared_ptr<INotificationDelivery> handler) {
    notification_handler_ = handler;
}

std::vector<QodSession> QodSessionManager::get_all_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    std::vector<QodSession> result;
    result.reserve(sessions_.size());
    
    for (const auto& [id, session] : sessions_) {
        result.push_back(session);
    }
    
    return result;
}

std::vector<QodSession> QodSessionManager::get_sessions_by_status(QosStatus status) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    std::vector<QodSession> result;
    
    for (const auto& [id, session] : sessions_) {
        if (session.qos_status == status) {
            result.push_back(session);
        }
    }
    
    return result;
}

} // namespace qod
} // namespace af