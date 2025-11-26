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
#include "models/ue_state.h"

namespace af {
namespace qod {

QodSessionManager::QodSessionManager(
    std::shared_ptr<UeStateManager> ue_state_manager,
    std::shared_ptr<QodStateManager> qod_state_manager,
    const QodSessionConfig& config)
    : ue_state_manager_(ue_state_manager), qod_state_manager_(qod_state_manager), config_(config) {

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

    // Clean up all sessions in qod_state_manager_
    {
        qod_state_manager_->clear_all_sessions();
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

std::optional<af::common::qod::QodSession> QodSessionManager::create_session(
    const af::common::qod::CreateSessionRequest& request) {

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

    // Get QoS profile mapping
    auto mapping_opt = qod_state_manager_->get_qos_profile_mapping(request.qos_profile);
    if (!mapping_opt || !mapping_opt.has_value()) {
        logger_->error("No mapping found for QoS profile: {}", request.qos_profile);
        return std::nullopt;
    }
    auto mapping = mapping_opt.value();

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
    af::common::qod::QodSession session;
    session.session_id = generate_session_id();
    session.api_consumer_id = request.api_consumer_id;
    session.device = request.device;
    session.application_server = request.application_server;
    session.device_ports = request.device_ports;
    session.application_server_ports = request.application_server_ports;
    session.qos_profile = request.qos_profile;
    session.qos_profile_mapping = mapping;
    session.duration = request.duration;
    session.sink = request.sink;
    session.sink_credential = request.sink_credential;
    session.created_at = std::chrono::system_clock::now();
    session.qos_status = af::common::qod::QosStatus::REQUESTED;
    session.ue_supi = resolved_supi;
    session.pdu_session_id = pdu_session_id;

    // Determine which device identifier to return in response
    if (request.device && request.api_consumer_id.find("2-legged") != std::string::npos) {
        // For 2-legged tokens, return one device identifier
        // Priority: phone_number > ipv4_address > ipv6_address
        af::common::qod::QodDevice response_device;
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

    // Store session and create/update UE state
    {
        qod_state_manager_->add_session(session);

        // Create or update UE state in the UE State Manager
        create_or_update_ue_state(session);
    }

    // Apply to PCF
    bool pcf_result = apply_session_to_pcf(session);

    if (!pcf_result) {
        // PCF application failed immediately
        logger_->error("Failed to apply session to PCF");

        // Update session status
        {
            qod_state_manager_->update_session_status(session.session_id, af::common::qod::QosStatus::UNAVAILABLE, af::common::qod::StatusInfo::NETWORK_TERMINATED);

        }

        auto state_session_opt = qod_state_manager_->get_session_by_id(session.session_id);
        if (!state_session_opt || !state_session_opt.has_value()) {
            logger_->debug("Session not found: {}", session.session_id);
        }

        af::common::qod::QodSession state_session = state_session_opt.value();
        // Send notification if configured
        if (request.sink && config_.enable_notifications) {
            send_status_change_notification(
                state_session,
                af::common::qod::QosStatus::REQUESTED,
                af::common::qod::StatusInfo::NETWORK_TERMINATED);
        }

        return state_session;
    }

    logger_->info("QoD session created with ID: {}", session.session_id);

    // Return the session in REQUESTED state
    // PCF will asynchronously confirm when it's AVAILABLE
    return session;
}

std::optional<af::common::qod::QodSession> QodSessionManager::get_session(
    const std::string& session_id,
    const std::string& api_consumer_id) {

    logger_->info("Retrieving session: {}", session_id);
    auto session_opt = qod_state_manager_->get_session_by_id(session_id);
    if (!session_opt) {
        logger_->debug("Session not found: {}", session_id);
        return std::nullopt;
    }

    // Check authorization
    if (session_opt->api_consumer_id != api_consumer_id) {
        logger_->warn("Unauthorized access to session {} by consumer {}",
                     session_id, api_consumer_id);
        return std::nullopt;
    }
    return *session_opt;
}

bool QodSessionManager::delete_session(
    const std::string& session_id,
    const std::string& api_consumer_id) {

    logger_->info("Deleting session: {}", session_id);

    auto session_opt = qod_state_manager_->get_session_by_id(session_id);
    if (!session_opt || !session_opt.has_value()) {
        logger_->debug("Session not found: {}", session_id);
        return false;
    }
    auto session = session_opt.value();

    // Check authorization
    if (session.api_consumer_id != api_consumer_id) {
        logger_->warn("Unauthorized deletion attempt for session {} by consumer {}",
                     session_id, api_consumer_id);
        return false;
    }


    // af::common::qod::QodSession& session = it->second;
    af::common::qod::QosStatus old_status = session.qos_status;

    bool deleted = false;
    // Remove from PCF if active
    if (session.qos_status == af::common::qod::QosStatus::AVAILABLE) {
        deleted = remove_session_from_pcf(session);
    }

    if (!deleted) {
        logger_->error("Failed to remove session {} from PCF", session_id);
        return false;
    }

    // Update status
    session.qos_status = af::common::qod::QosStatus::UNAVAILABLE;
    session.status_info = af::common::qod::StatusInfo::DELETE_REQUESTED;
    session.expires_at = std::chrono::system_clock::now();

    // Remove QoD session from UE state
    if (session.pdu_session_id && ue_state_manager_) {
        std::optional<UeKey> key = create_or_get_ue_key(session);
        if (!key) {
            logger_->error("Failed to create UE key for session: {}", session.session_id);
            return false;
        }
        ue_state_manager_->remove_qod_session_from_pdu_session(
            *key, *session.pdu_session_id, session.session_id);
        logger_->debug("Removed QoD session {} from UE state", session_id);
    }

    // Send notification if configured
    if (session.sink && config_.enable_notifications && old_status == af::common::qod::QosStatus::AVAILABLE) {
        send_status_change_notification(session, old_status, af::common::qod::StatusInfo::DELETE_REQUESTED);
    }

    // Mark for cleanup (will be removed after TTL)
    // This allows polling clients to see the UNAVAILABLE status

    logger_->info("Session {} marked as deleted", session_id);
    return true;
}

std::optional<af::common::qod::QodSession> QodSessionManager::extend_session_duration(
    const af::common::qod::ExtendSessionDurationRequest& request,
    const std::string& api_consumer_id) {

    logger_->info("Extending session {} by {} seconds",
                 request.session_id, request.requested_additional_duration.count());

    auto session_opt = qod_state_manager_->get_session_by_id(request.session_id);
    if (!session_opt || !session_opt.has_value()) {
        logger_->debug("Session not found: {}", request.session_id);
        return std::nullopt;
    }

    auto session = session_opt.value();

    // Check authorization
    if (session.api_consumer_id != api_consumer_id) {
        logger_->warn("Unauthorized extension attempt for session {} by consumer {}",
                     request.session_id, api_consumer_id);
        return std::nullopt;
    }

    // Can only extend AVAILABLE sessions
    if (session.qos_status != af::common::qod::QosStatus::AVAILABLE) {
        logger_->error("Cannot extend session {} in status {}",
                      request.session_id, af::common::qod::QodTypeUtils::qos_status_to_string(session.qos_status));
        return std::nullopt;
    }

    // Calculate new duration
    auto new_total_duration = session.duration + request.requested_additional_duration;

    // Check against maximum duration for profile
    auto max_duration = get_max_duration_for_profile(session.qos_profile);
    if (new_total_duration > max_duration) {
        new_total_duration = max_duration;
        logger_->info("Capping extended duration to maximum: {} seconds", max_duration.count());
    }

    // Update session
    session.duration = new_total_duration;
    session.expires_at = *session.started_at + new_total_duration;

    logger_->info("Session {} extended to {} seconds total duration now expires at {}",
                 request.session_id, new_total_duration.count(), session.expires_at->time_since_epoch().count());

    // Update in state manager
    {
        qod_state_manager_->update_session(session);
    }

    return session;
}

std::vector<af::common::qod::QodSession> QodSessionManager::retrieve_sessions_by_device(
    const af::common::qod::RetrieveSessionsRequest& request) {

    logger_->debug("Retrieving sessions for device");

    std::vector<af::common::qod::QodSession> result;

    // Get all sessions from state manager
    auto all_sessions = qod_state_manager_->get_all_sessions();

    for (const auto& session : all_sessions) {
        // Skip sessions that don't belong to this API consumer
        if (session.api_consumer_id != request.api_consumer_id) {
            continue;
        }

        // Skip unavailable sessions (only return REQUESTED or AVAILABLE)
        if (session.qos_status == af::common::qod::QosStatus::UNAVAILABLE) {
            continue;
        }

        // If no device filter specified, include all sessions for this API consumer
        if (!request.device) {
            result.push_back(session);
            continue;
        }

        // Device filter specified - check if session matches the device
        if (!session.device) {
            // Session has no device info, skip it
            continue;
        }

        bool device_matches = false;

        // Check phone number match
        if (request.device->phone_number && session.device->phone_number) {
            if (*request.device->phone_number == *session.device->phone_number) {
                device_matches = true;
            }
        }

        // Check IPv4 address match
        if (!device_matches && request.device->ipv4_address && session.device->ipv4_address) {
            // Compare public addresses
            if (request.device->ipv4_address->public_address.value == session.device->ipv4_address->public_address.value) {
                device_matches = true;
            }
            // Compare private addresses if both present
            else if (request.device->ipv4_address->private_address && session.device->ipv4_address->private_address) {
                if (request.device->ipv4_address->private_address->value == session.device->ipv4_address->private_address->value) {
                    device_matches = true;
                }
            }
        }

        // Check IPv6 address match
        if (!device_matches && request.device->ipv6_address && session.device->ipv6_address) {
            if (request.device->ipv6_address->value == session.device->ipv6_address->value) {
                device_matches = true;
            }
        }

        // If device matches, include this session
        if (device_matches) {
            result.push_back(session);
        }
    }

    logger_->debug("Found {} sessions for device", result.size());
    return result;
}

// === PCF Integration ===

// TODO: Add optional payload that might be passed by PCF i.e., the UE Identifiers
void QodSessionManager::handle_pcf_session_response(
    const std::string& qod_session_id,
    const std::string& pcf_session_id,
    bool success,
    const std::string& error_message) {

    logger_->info("PCF response for session {}: success={}, pcf_id={}",
                 qod_session_id, success, pcf_session_id);

    auto session_opt = qod_state_manager_->get_session_by_id(qod_session_id);
    if (!session_opt || !session_opt.has_value()) {
        logger_->error("Session not found: {}", qod_session_id);
        return;
    }

    af::common::qod::QodSession& session = session_opt.value();
    af::common::qod::QosStatus old_status = session.qos_status;

    if (success) {
        // Session is now available
        session.qos_status = af::common::qod::QosStatus::AVAILABLE;
        session.pcf_session_id = pcf_session_id;
        session.started_at = std::chrono::system_clock::now();
        session.expires_at = *session.started_at + session.duration;

        // Update UE state when PCF confirms the session
        // This might provide additional UE information from network
        update_ue_state_from_pcf_response(session);

        // Map PCF ID to QoD ID
        {
            qod_state_manager_->update_pcf_session_map(pcf_session_id, qod_session_id);
        }

        // Send notification
        if (session.sink && config_.enable_notifications) {
            send_status_change_notification(session, old_status, std::nullopt);
        }
    } else {
        // Session failed
        session.qos_status = af::common::qod::QosStatus::UNAVAILABLE;
        session.status_info = af::common::qod::StatusInfo::NETWORK_TERMINATED;
        session.error_message = error_message;

        // Send notification
        if (session.sink && config_.enable_notifications) {
            send_status_change_notification(session, old_status, af::common::qod::StatusInfo::NETWORK_TERMINATED);
        }
    }

    // Update session in state manager
    {
        qod_state_manager_->update_session(session);
    }
}

void QodSessionManager::handle_pcf_session_terminated(
    const std::string& pcf_session_id,
    const std::string& reason) {

    logger_->info("PCF session terminated: pcf_id={}, reason={}", pcf_session_id, reason);

    auto session_opt = qod_state_manager_->get_session_by_pcf_session_id(pcf_session_id);
    if (!session_opt || !session_opt.has_value()) {
        logger_->error("QoD session not found for PCF session ID: {}", pcf_session_id);
        return;
    }


    af::common::qod::QodSession& session = session_opt.value();

    if (session.qos_status == af::common::qod::QosStatus::AVAILABLE) {
        af::common::qod::QosStatus old_status = session.qos_status;
        session.qos_status = af::common::qod::QosStatus::UNAVAILABLE;
        session.status_info = af::common::qod::StatusInfo::NETWORK_TERMINATED;
        session.expires_at = std::chrono::system_clock::now();

        // Remove QoD session from UE state when terminated by network
        if (session.pdu_session_id && ue_state_manager_) {
            std::optional<UeKey> key = create_or_get_ue_key(session);
            if (!key) {
                logger_->error("Failed to create UE key for session: {}", session.session_id);
                return;
            }
            ue_state_manager_->remove_qod_session_from_pdu_session(
                *key, *session.pdu_session_id, session.session_id);
            logger_->debug("Removed QoD session {} from UE state", session.session_id);
        }

        // Assume the non-primary indentifier of IPv4/IPv6 are no longer valid
        // TODO: This might not always be the case if UE has multiple PDU sessions
        // TODO [3GPP]: Verify if the identifier is still used by other sessions before removing
        // and what the 3GPP spec says about this
        if (session.device && ue_state_manager_) {
            std::optional<UeKey> key = create_or_get_ue_key(session);
            if (key) {
                // Check for IPv4 or IPv6 address and remove
                if (session.device->ipv4_address) {
                    ue_state_manager_->remove_non_primary_ue_identifier(*key, "ipv4", session.device->ipv4_address->public_address.value);
                }
                if (session.device->ipv6_address) {
                    ue_state_manager_->remove_non_primary_ue_identifier(*key, "ipv6", session.device->ipv6_address->value);
                }
                logger_->debug("Removed non-primary UE identifier for session {}", session.session_id);
            }
        }

        // Send notification
        if (session.sink && config_.enable_notifications) {
            send_status_change_notification(session, old_status, af::common::qod::StatusInfo::NETWORK_TERMINATED);
        }
    }

    // Remove PCF mapping
    {
        qod_state_manager_->remove_session(session.session_id);
        logger_->debug("Removed QoD session {} after PCF termination", session.session_id);

        qod_state_manager_->remove_pcf_to_qod_session_mapping(pcf_session_id);
        logger_->debug("Removed PCF to QoD session mapping for PCF ID: {}", pcf_session_id);
    }
}

void QodSessionManager::handle_pdu_session_terminated_event(
    const af::core::events::PduSessionTerminatedEvent& event) {

    logger_->info("Handling PDU Session Terminated Event for SUPI: {}, PDU ID: {}",
                 event.supi.value, event.pdu_session_id);

    // First get session and check if there is sink configured
    auto session_opt = qod_state_manager_->get_session_by_pdu_session(event.supi, event.pdu_session_id);
    if (!session_opt || !session_opt.has_value()) {
        logger_->debug("No QoD session associated with SUPI: {}, PDU ID: {}",
                      event.supi.value, event.pdu_session_id);
        return;
    }
    af::common::qod::QodSession& qod_session = session_opt.value();

    // Delete the session if it exists
    qod_state_manager_->remove_session(qod_session.session_id);

    if (qod_session.sink && config_.enable_notifications && qod_session.qos_status == af::common::qod::QosStatus::AVAILABLE) {
        logger_->info("Sending notification for QoD session {} due to PDU session termination", qod_session.session_id);
        send_status_change_notification(qod_session, qod_session.qos_status, af::common::qod::StatusInfo::NETWORK_TERMINATED);
    }
}

// === Private Methods ===

std::pair<std::optional<Supi>, std::optional<std::string>>
QodSessionManager::resolve_device(const af::common::qod::QodDevice& device) {
    logger_->debug("Resolving device to SUPI");

    if (!ue_state_manager_) {
        logger_->error("UE State Manager not initialized");
        return {std::nullopt, std::nullopt};
    }

    std::optional<AfUeSubscriptionState> ue_state;

    // Try to resolve by different identifiers using the new UE State Manager
    if (device.phone_number) {
        // Convert phone number to GPSI format
        Gpsi gpsi{*device.phone_number};
        ue_state = ue_state_manager_->get_ue_state_by_gpsi(gpsi);
        if (!ue_state) {
            logger_->debug("No UE state found for phone number: {}", device.phone_number.value());
        }
    }

    if (!ue_state && device.ipv4_address.has_value()) {
        logger_->debug("Attempting to resolve by IPv4 address");
        if (!device.ipv4_address->public_address.value.empty()) {
            Ipv4Addr addr{device.ipv4_address->public_address.value};
            ue_state = ue_state_manager_->get_ue_state_by_ipv4(addr);
        }
        if (!ue_state && device.ipv4_address->private_address) {
            Ipv4Addr addr{device.ipv4_address->private_address->value};
            ue_state = ue_state_manager_->get_ue_state_by_ipv4(addr);
        }
        if (!ue_state) {
            logger_->debug("No UE state found for provided IPv4 addresses {}", device.ipv4_address->public_address.value);
        }
    }

    if (!ue_state && device.ipv6_address) {
        Ipv6Addr addr{device.ipv6_address->value};
        ue_state = ue_state_manager_->get_ue_state_by_ipv6_addr(addr);
        if (!ue_state) {
            logger_->debug("No UE state found for IPv6 address: {}", device.ipv6_address->value);
        }
    }

    if (ue_state) {
        // Get the first active PDU session if available
        std::optional<std::string> pdu_session_id;
        if (!ue_state->get_pdu_sessions().empty()) {
            for (const auto& pdu_session : ue_state->get_pdu_sessions()) {
                if (pdu_session.status == "ACTIVE") {
                    pdu_session_id = pdu_session.pdu_session_id;
                    break;
                }
            }
        }
        return {ue_state->get_supi(), pdu_session_id};
    } else {
        logger_->warn("Could not resolve device to SUPI");
    }

    return {std::nullopt, std::nullopt};
}

std::optional<UeKey> QodSessionManager::create_or_get_ue_key(const af::common::qod::QodSession& session) {
    if (!ue_state_manager_ || !session.device) {
        logger_->error("UE State Manager not initialized or session has no device");
        return std::nullopt;
    }

    // Determine the best UeKey for this session
    std::optional<UeKey> ue_key;

    // Priority: SUPI > GPSI (phone_number) > IPv4 > IPv6
    if (session.ue_supi) {
        ue_key = UeKey(*session.ue_supi);
        logger_->debug("Using SUPI-based UeKey: {}", session.ue_supi->value);
    } else if (session.device->phone_number) {
        Gpsi gpsi{*session.device->phone_number};
        ue_key = UeKey(gpsi);
        logger_->debug("Using GPSI-based UeKey: {}", *session.device->phone_number);
    } else if (session.device->ipv4_address) {
        Ipv4Addr ipv4{session.device->ipv4_address->public_address.value};
        ue_key = UeKey(ipv4);
        logger_->debug("Using IPv4-based UeKey: {}", session.device->ipv4_address->public_address.value);
    } else if (session.device->ipv6_address) {
        Ipv6Addr ipv6{session.device->ipv6_address->value};
        ue_key = UeKey(ipv6);
        logger_->debug("Using IPv6-based UeKey: {}", session.device->ipv6_address->value);
    }

    if (!ue_key) {
        logger_->error("No valid identifier found to create UE key for session: {}", session.session_id);
        return std::nullopt;
    }

    return *ue_key;
}

void QodSessionManager::create_or_update_ue_state(const af::common::qod::QodSession& session) {
    if (!ue_state_manager_ || !session.device) {
        return;
    }

    logger_->debug("Creating/updating UE state for QoD session: {}", session.session_id);

    try {
        // Determine the best UeKey for this session
        std::optional<UeKey> ue_key;

        // Priority: SUPI > GPSI (phone_number) > IPv4 > IPv6
        if (session.ue_supi) {
            ue_key = UeKey(*session.ue_supi);
            logger_->debug("Using SUPI-based UeKey: {}", session.ue_supi->value);
        } else if (session.device->phone_number) {
            Gpsi gpsi{*session.device->phone_number};
            ue_key = UeKey(gpsi);
            logger_->debug("Using GPSI-based UeKey: {}", *session.device->phone_number);
        } else if (session.device->ipv4_address) {
            Ipv4Addr ipv4{session.device->ipv4_address->public_address.value};
            ue_key = UeKey(ipv4);
            logger_->debug("Using IPv4-based UeKey: {}", session.device->ipv4_address->public_address.value);
        } else if (session.device->ipv6_address) {
            Ipv6Addr ipv6{session.device->ipv6_address->value};
            ue_key = UeKey(ipv6);
            logger_->debug("Using IPv6-based UeKey: {}", session.device->ipv6_address->value);
        }

        if (!ue_key) {
            logger_->warn("No valid identifier found to create UE key for session: {}", session.session_id);
            return;
        }

        // Create initial UE state data
        AfUeSubscriptionState initial_state = session.ue_supi
            ? AfUeSubscriptionState(*session.ue_supi)  // SUPI-based resolved state
            : AfUeSubscriptionState(*ue_key);           // Provisional state

        // If SUPI is available but state was created from key, resolve it
        if (session.ue_supi && !initial_state.get_supi()) {
            initial_state.resolve_supi(*session.ue_supi);
        }

        // Set GPSI if available
        if (session.device->phone_number) {
            Gpsi gpsi{*session.device->phone_number};
            initial_state.set_gpsi(gpsi);
        }

        // Add known identifiers for cross-referencing
        if (session.device->ipv4_address) {
            initial_state.add_known_identifier("ipv4", session.device->ipv4_address->public_address.value);
            if (session.device->ipv4_address->private_address) {
                initial_state.add_known_identifier("ipv4", session.device->ipv4_address->private_address->value);
            }
        }
        if (session.device->ipv6_address) {
            initial_state.add_known_identifier("ipv6", session.device->ipv6_address->value);
        }
        if (session.device->phone_number) {
            initial_state.add_known_identifier("gpsi", *session.device->phone_number);
        }

        // Create or update the UE state
        auto& ue_state = ue_state_manager_->create_or_update_provisional_ue_state(*ue_key, initial_state);

        logger_->info("UE state created/updated for session {} with key type: {}",
                     session.session_id, static_cast<int>(ue_key->get_type()));

        // If we have a PDU session, add it to the UE state
        if (session.pdu_session_id && (session.device->ipv4_address || session.device->ipv6_address)) {
            PduSessionData pdu_data;
            pdu_data.pdu_session_id = *session.pdu_session_id;
            pdu_data.status = "ACTIVE"; // Assume active for QoD sessions
            pdu_data.pdu_session_type = "IPV4"; // Default, could be IPV6 or ETHERNET

            if (session.device->ipv4_address) {
                pdu_data.ue_ipv4_address = Ipv4Addr{session.device->ipv4_address->public_address.value};
                pdu_data.pdu_session_type = "IPV4";
            }
            if (session.device->ipv6_address) {
                pdu_data.ue_ipv6_addresses.push_back(Ipv6Addr{session.device->ipv6_address->value});
                pdu_data.pdu_session_type = session.device->ipv4_address ? "IPV4V6" : "IPV6";
            }

            // Add QoD session to the PDU session
            pdu_data.active_qod_session_ids.insert(session.session_id);

            ue_state_manager_->add_or_update_pdu_session(*ue_key, pdu_data);

            logger_->debug("PDU session {} added to UE state for QoD session {}",
                          *session.pdu_session_id, session.session_id);
        }

    } catch (const std::exception& e) {
        logger_->error("Error creating/updating UE state for session {}: {}",
                      session.session_id, e.what());
    }
}

void QodSessionManager::update_ue_state_from_pcf_response(const af::common::qod::QodSession& session) {
    if (!ue_state_manager_ || !session.device) {
        return;
    }

    logger_->debug("Updating UE state from PCF response for session: {}", session.session_id);

    try {
        // If we now have a SUPI from PCF response but didn't before, promote the state
        if (session.ue_supi) {
            // Try to find existing provisional state by other identifiers and promote it
            std::optional<UeKey> current_key;

            // Find current provisional state by device identifiers in their priority order
            // Priority: GPSI (phone_number) > IPv4 > IPv6
            if (session.device->phone_number) {
                Gpsi gpsi{*session.device->phone_number};
                current_key = UeKey(gpsi);
            } else if (session.device->ipv4_address) {
                Ipv4Addr ipv4{session.device->ipv4_address->public_address.value};
                current_key = UeKey(ipv4);
            } else if (session.device->ipv6_address) {
                Ipv6Addr ipv6{session.device->ipv6_address->value};
                current_key = UeKey(ipv6);
            }

            if (current_key) {
                // Check if we need to promote from provisional to SUPI-based
                UeKey supi_key(*session.ue_supi);
                if (current_key->get_primary_key() != supi_key.get_primary_key()) {
                    bool promoted = ue_state_manager_->promote_ue_state_to_resolved(*current_key, *session.ue_supi);
                    if (promoted) {
                        logger_->info("Promoted UE state from {} to SUPI-based for session {}",
                                     current_key->get_primary_key(), session.session_id);
                    } else {
                        // If promotion failed, there might be a conflict, try merging
                        auto existing_supi_state = ue_state_manager_->get_ue_state_by_supi(*session.ue_supi);
                        if (existing_supi_state) {
                            bool merged = ue_state_manager_->merge_ue_states(supi_key, *current_key);
                            if (merged) {
                                logger_->info("Merged UE states for session {}: {} -> {}",
                                             session.session_id, current_key->get_primary_key(), supi_key.get_primary_key());
                            }
                        }
                    }
                }
            }
        }

        // Update QoD session association with PDU session if we have the information
        if (session.pdu_session_id) {
            // Determine the best UeKey for this session
            std::optional<UeKey> ue_key;
            // Priority: SUPI > GPSI (phone_number) > IPv4 > IPv6
            if (session.ue_supi) {
                ue_key = UeKey(*session.ue_supi);
            } else if (session.device->phone_number) {
                Gpsi gpsi{*session.device->phone_number};
                ue_key = UeKey(gpsi);
            } else if (session.device->ipv4_address) {
                Ipv4Addr ipv4{session.device->ipv4_address->public_address.value};
                ue_key = UeKey(ipv4);
            } else if (session.device->ipv6_address) {
                Ipv6Addr ipv6{session.device->ipv6_address->value};
                ue_key = UeKey(ipv6);
            }
            if (ue_key) {
                ue_state_manager_->add_qod_session_to_pdu_session(
                    *ue_key, *session.pdu_session_id, session.session_id);

                logger_->debug("Updated QoD session association in UE state for key: {}, PDU: {}, QoD: {}",
                              ue_key->get_primary_key(), *session.pdu_session_id, session.session_id);
            }
        }

    } catch (const std::exception& e) {
        logger_->error("Error updating UE state from PCF response for session {}: {}",
                      session.session_id, e.what());
    }
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
    const af::common::qod::QodDevice& device,
    const af::common::qod::ApplicationServer& app_server) {

    auto sessions = retrieve_sessions_by_device(
        af::common::qod::RetrieveSessionsRequest{device, ""});

    for (const auto& session : sessions) {
        // Only check REQUESTED or AVAILABLE sessions
        if (session.qos_status == af::common::qod::QosStatus::UNAVAILABLE) {
            continue;
        }
        // Check if same application server
        bool same_server = false;
        if (app_server.ipv4_address && session.application_server.ipv4_address) {
            same_server = (*app_server.ipv4_address == *session.application_server.ipv4_address);
        } else if (app_server.ipv6_address && session.application_server.ipv6_address) {
            same_server = (*app_server.ipv6_address == *session.application_server.ipv6_address);
        }
        if (same_server) {
            return session.session_id;  // Conflict found
        }
    }

    return std::nullopt;
}

bool QodSessionManager::apply_session_to_pcf(af::common::qod::QodSession& session) {
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
        auto pcf_request = build_pcf_request(session); // TODO: pass QodSession instead??

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

        // TODO: Create shared definition for PCF response message types
        // Process immediate response if synchronous
        if (response->message_type == "pcf_qod_session_created") {
            std::string response_str(response->payload.begin(), response->payload.end());
            auto response_json = nlohmann::json::parse(response_str);

            std::string pcf_session_id = response_json.value("pcf_session_id", "");
            handle_pcf_session_response(session.session_id, pcf_session_id, true);
        } else if (response->message_type == "pcf_delete_app_session") {
            // Call handle_pcf_session_terminated
            std::string response_str(response->payload.begin(), response->payload.end());
            auto response_json = nlohmann::json::parse(response_str);
            std::string pcf_session_id = response_json.value("pcf_session_id", "");
            std::string reason = response_json.value("reason", "Requested by user");
            handle_pcf_session_terminated(pcf_session_id, reason);

        } else if (response->message_type == "pcf_error") {
            std::string error_str(response->payload.begin(), response->payload.end());
            auto error_json = nlohmann::json::parse(error_str);

            std::string error_message = error_json.value("message", "Unknown error");
            handle_pcf_session_response(session.session_id, "", false, error_message);
            return false;
        } else {
            logger_->warn("Received unexpected response from PCF: {}", response->message_type);
        }

        return true;
    }
    catch (const std::exception& e) {
        logger_->error("Error applying session to PCF: {}", e.what());
        return false;
    }
}

bool QodSessionManager::remove_session_from_pcf(const af::common::qod::QodSession& session) {
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
            {"pcf_session_id", *session.pcf_session_id},
            {"session_id", session.session_id}
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

        if (response && response->message_type == "pcf_delete_app_session") {

            std::string response_str(response->payload.begin(), response->payload.end());
            auto response_json = nlohmann::json::parse(response_str);
            std::string pcf_session_id = response_json.value("pcf_session_id", "");
            std::string reason = response_json.value("reason", "Requested by user");
            handle_pcf_session_terminated(pcf_session_id, reason);

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

void QodSessionManager::send_status_change_notification(
    const af::common::qod::QodSession& session,
    af::common::qod::QosStatus old_status,
    std::optional<af::common::qod::StatusInfo> status_info) {

    if (!session.sink || !notification_handler_) {
        return;
    }

    logger_->info("Sending status change notification for session {}: {} -> {}",
                 session.session_id,
                 af::common::qod::QodTypeUtils::qos_status_to_string(old_status),
                 af::common::qod::QodTypeUtils::qos_status_to_string(session.qos_status));

    // Create CloudEvent
    auto event = af::common::qod::QodEventBuilder::create_qos_status_changed_event(
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

    auto to_remove_sessions = qod_state_manager_->get_expired_sessions(now, config_.unavailable_session_ttl);
    for (const auto& session : to_remove_sessions) {
        logger_->debug("Cleaning up expired session: {}", session.session_id);
        to_remove.push_back(session.session_id);
        qod_state_manager_->remove_session(session.session_id);
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

nlohmann::json QodSessionManager::build_pcf_request(const af::common::qod::QodSession& session) {
    nlohmann::json request;

    // Basic session information
    request["session_id"] = session.session_id;
    request["qos_profile"] = session.qos_profile;
    request["duration"] = session.duration.count();

    // Device information
    if (session.device) {
        request["device"] = convert_device_to_pcf(*session.device, session.ue_supi);
    } else if (session.ue_supi) {
        request["ue_supi"] = session.ue_supi->value;
    }

    // Application server
    if (session.application_server.ipv4_address) {
        request["application_server"]["ipv4Address"] = *session.application_server.ipv4_address;
    }
    if (session.application_server.ipv6_address) {
        request["application_server"]["ipv6Address"] = *session.application_server.ipv6_address;
    }

    // Device ports
    if (session.device_ports) {
        // Convert PortsSpec to json object
        std::vector<nlohmann::json> port_ranges;
        for (const auto& range : session.device_ports->ranges) {
            port_ranges.push_back({
                {"from", range.from},
                {"to", range.to}
            });
        }
        if (!port_ranges.empty()) {
            request["device_ports"]["port_ranges"] = port_ranges;
        }

        if (!session.device_ports->ports.empty()) {
            request["device_ports"]["ports"] = session.device_ports->ports;
        }
    }

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
    const af::common::qod::QodDevice& device,
    const std::optional<Supi>& supi) {

    nlohmann::json pcf_device;

    if (supi) {
        pcf_device["supi"] = supi->value;
    }

    if (device.phone_number) {
        pcf_device["gpsi"] = *device.phone_number;
    }

    if (device.ipv4_address) {
        pcf_device["ipv4Address"] = {};
        pcf_device["ipv4Address"]["publicAddress"] = device.ipv4_address->public_address.value;
        if (device.ipv4_address->private_address) {
            pcf_device["ipv4Address"]["privateAddress"] = device.ipv4_address->private_address->value;
        }
        if (device.ipv4_address->public_port) {
            pcf_device["ipv4Address"]["publicPort"] = *device.ipv4_address->public_port;
        }
    }

    if (device.ipv6_address) {
        pcf_device["ipv6Address"] = device.ipv6_address->value;
    }

    return pcf_device;
}

nlohmann::json QodSessionManager::build_flow_filters(const af::common::qod::QodSession& session) {
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
    std::shared_ptr<af::common::qod::INotificationDelivery> handler) {
    notification_handler_ = handler;
}

} // namespace qod
} // namespace af