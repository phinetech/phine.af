/// @file qod_client.cpp
/// @brief QodClient implementation — gRPC client for CAMARA QoD on af_core.

#include "qod_client.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <thread>

using json = nlohmann::json;

namespace phine::adapter {

// ─── Construction ───────────────────────────────────────────────────────────

QodClient::QodClient(const QodClientConfig& config)
    : config_(config),
      channel_(grpc::CreateChannel(config.af_core_address,
                                   grpc::InsecureChannelCredentials())),
      stub_(af::proto::InternalCommunication::NewStub(channel_)) {
    spdlog::info("[QodClient] Created — target: {}", config_.af_core_address);
}

// ─── Connection readiness ───────────────────────────────────────────────────

bool QodClient::wait_for_ready(int timeout_seconds) {
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(timeout_seconds);

    spdlog::info("[QodClient] Waiting for af_core at {} (timeout {}s)…",
                 config_.af_core_address, timeout_seconds);

    while (std::chrono::system_clock::now() < deadline) {
        auto state = channel_->GetState(/*try_to_connect=*/true);
        if (state == GRPC_CHANNEL_READY) {
            spdlog::info("[QodClient] Channel READY");
            return true;
        }
        // Wait for a state change or 1-second timeout, whichever comes first.
        channel_->WaitForStateChange(
            state,
            std::chrono::system_clock::now() + std::chrono::seconds(1));
    }
    spdlog::error("[QodClient] Timeout waiting for channel readiness");
    return false;
}

// ─── Public API ─────────────────────────────────────────────────────────────

Result<SessionInfo> QodClient::create_session(
    const CreateSessionRequest& request) {
    auto payload = serialise_create_request(request);

    spdlog::debug("[QodClient] create_session payload: {}", payload);

    auto result = send_message("qod_create_session", payload,
                               {{"source", "demo-qod-adapter"}});
    if (!result.success) {
        return Result<SessionInfo>::error(result.error_code,
                                          result.error_message);
    }
    return Result<SessionInfo>::ok(parse_session_info(result.value));
}

Result<SessionInfo> QodClient::get_session(const std::string& session_id) {
    auto result = send_message("qod_get_session", "",
                               {{"session_id", session_id},
                                {"source", "demo-qod-adapter"}});
    if (!result.success) {
        return Result<SessionInfo>::error(result.error_code,
                                          result.error_message);
    }
    return Result<SessionInfo>::ok(parse_session_info(result.value));
}

Result<void> QodClient::delete_session(const std::string& session_id) {
    auto result = send_message("qod_delete_session", "",
                               {{"session_id", session_id},
                                {"source", "demo-qod-adapter"}});
    if (!result.success) {
        return Result<void>::error(result.error_code, result.error_message);
    }
    return Result<void>::ok();
}

Result<SessionInfo> QodClient::extend_session(
    const std::string& session_id, const ExtendSessionRequest& request) {
    auto payload = serialise_extend_request(request);

    auto result = send_message("qod_extend_session", payload,
                               {{"session_id", session_id},
                                {"source", "demo-qod-adapter"}});
    if (!result.success) {
        return Result<SessionInfo>::error(result.error_code,
                                          result.error_message);
    }
    return Result<SessionInfo>::ok(parse_session_info(result.value));
}

Result<std::vector<SessionInfo>> QodClient::retrieve_sessions(
    const Device& device) {
    auto payload = serialise_device(device);

    auto result = send_message("qod_retrieve_sessions", payload,
                               {{"source", "demo-qod-adapter"}});
    if (!result.success) {
        return Result<std::vector<SessionInfo>>::error(result.error_code,
                                                       result.error_message);
    }
    return Result<std::vector<SessionInfo>>::ok(
        parse_session_list(result.value));
}

// ─── send_message (with retry) ──────────────────────────────────────────────

Result<std::string> QodClient::send_message(
    const std::string& message_type, const std::string& json_payload,
    const std::map<std::string, std::string>& metadata) {
    int delay_ms = config_.initial_retry_delay_ms;

    for (int attempt = 1; attempt <= config_.max_retries; ++attempt) {
        // Build request
        af::proto::InternalMessage request;
        request.set_message_type(message_type);
        request.set_correlation_id(generate_correlation_id());
        if (!json_payload.empty()) {
            request.set_payload(json_payload.data(), json_payload.size());
        }
        for (const auto& [k, v] : metadata) {
            (*request.mutable_metadata())[k] = v;
        }

        // Set deadline
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                             std::chrono::seconds(config_.timeout_seconds));

        af::proto::InternalMessage response;
        grpc::Status status =
            stub_->SendMessage(&context, request, &response);

        if (status.ok()) {
            std::string resp_payload(response.payload().begin(),
                                     response.payload().end());
            spdlog::debug("[QodClient] Response for {}: {}", message_type,
                          resp_payload);

            // Check for CAMARA-level error in the response
            if (!resp_payload.empty()) {
                try {
                    auto j = json::parse(resp_payload);
                    if (j.contains("status") && j.contains("code") &&
                        j.contains("message")) {
                        // This is an error response
                        return Result<std::string>::error(
                            j["code"].get<std::string>(),
                            j["message"].get<std::string>());
                    }
                } catch (...) {
                    // Not JSON or not an error — fall through
                }
            }

            return Result<std::string>::ok(std::move(resp_payload));
        }

        spdlog::warn(
            "[QodClient] RPC {} failed (attempt {}/{}): {} — {}",
            message_type, attempt, config_.max_retries,
            static_cast<int>(status.error_code()), status.error_message());

        if (!is_retryable(status.error_code())) {
            return Result<std::string>::error(
                "GRPC_" + std::to_string(static_cast<int>(
                              status.error_code())),
                status.error_message());
        }

        if (attempt < config_.max_retries) {
            spdlog::info("[QodClient] Retrying in {}ms…", delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;  // exponential backoff
        }
    }

    return Result<std::string>::error("GRPC_RETRIES_EXHAUSTED",
                                       "All retry attempts failed");
}

bool QodClient::is_retryable(grpc::StatusCode code) {
    return code == grpc::StatusCode::UNAVAILABLE ||
           code == grpc::StatusCode::DEADLINE_EXCEEDED ||
           code == grpc::StatusCode::RESOURCE_EXHAUSTED;
}

// ─── Correlation ID ─────────────────────────────────────────────────────────

std::string QodClient::generate_correlation_id() {
    // Simple UUID-v4 generator (good enough for demo / tracing)
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;

    auto hex = [](uint32_t v, int width) {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(width) << v;
        return ss.str();
    };

    return hex(dist(gen), 8) + "-" + hex(dist(gen) & 0xFFFF, 4) + "-4" +
           hex(dist(gen) & 0x0FFF, 3) + "-" +
           hex((dist(gen) & 0x3FFF) | 0x8000, 4) + "-" +
           hex(dist(gen), 8) + hex(dist(gen) & 0xFFFF, 4);
}

// ─── JSON Serialisation ─────────────────────────────────────────────────────

std::string QodClient::serialise_create_request(
    const CreateSessionRequest& req) {
    json j;

    // Device
    json dev;
    if (req.device.phone_number) {
        dev["phoneNumber"] = *req.device.phone_number;
    }
    if (req.device.network_access_identifier) {
        dev["networkAccessIdentifier"] = *req.device.network_access_identifier;
    }
    if (req.device.ipv4_address) {
        json ip;
        ip["publicAddress"] = req.device.ipv4_address->public_address;
        if (req.device.ipv4_address->public_port > 0) {
            ip["publicPort"] = req.device.ipv4_address->public_port;
        }
        dev["ipv4Address"] = ip;
    }
    j["device"] = dev;

    // Application server
    j["applicationServer"] = {
        {"ipv4Address", req.application_server.ipv4_address}};

    // Device ports
    if (req.device_ports) {
        json dp;
        if (!req.device_ports->ports.empty()) {
            dp["ports"] = req.device_ports->ports;
        }
        if (!req.device_ports->ranges.empty()) {
            json ranges = json::array();
            for (const auto& r : req.device_ports->ranges) {
                ranges.push_back({{"from", r.from}, {"to", r.to}});
            }
            dp["ranges"] = ranges;
        }
        j["devicePorts"] = dp;
    }

    // Application server ports
    if (req.application_server_ports) {
        json asp;
        if (!req.application_server_ports->ports.empty()) {
            asp["ports"] = req.application_server_ports->ports;
        }
        if (!req.application_server_ports->ranges.empty()) {
            json ranges = json::array();
            for (const auto& r : req.application_server_ports->ranges) {
                ranges.push_back({{"from", r.from}, {"to", r.to}});
            }
            asp["ranges"] = ranges;
        }
        j["applicationServerPorts"] = asp;
    }

    j["qosProfile"] = req.qos_profile;
    j["duration"] = req.duration_seconds;

    if (req.sink) {
        j["sink"] = *req.sink;
    }

    return j.dump();
}

std::string QodClient::serialise_extend_request(
    const ExtendSessionRequest& req) {
    json j;
    j["requestedAdditionalDuration"] = req.requested_additional_duration;
    return j.dump();
}

std::string QodClient::serialise_device(const Device& dev) {
    json j;
    json d;
    if (dev.phone_number) {
        d["phoneNumber"] = *dev.phone_number;
    }
    if (dev.network_access_identifier) {
        d["networkAccessIdentifier"] = *dev.network_access_identifier;
    }
    if (dev.ipv4_address) {
        json ip;
        ip["publicAddress"] = dev.ipv4_address->public_address;
        if (dev.ipv4_address->public_port > 0) {
            ip["publicPort"] = dev.ipv4_address->public_port;
        }
        d["ipv4Address"] = ip;
    }
    j["device"] = d;
    return j.dump();
}

SessionInfo QodClient::parse_session_info(const std::string& raw) {
    SessionInfo info;
    if (raw.empty()) return info;

    try {
        auto j = json::parse(raw);
        if (j.contains("sessionId")) {
            info.session_id = j["sessionId"].get<std::string>();
        }
        if (j.contains("qosStatus")) {
            info.qos_status =
                qos_status_from_string(j["qosStatus"].get<std::string>());
        }
        if (j.contains("statusInfo")) {
            info.status_info =
                status_info_from_string(j["statusInfo"].get<std::string>());
        }
        if (j.contains("duration")) {
            info.duration = j["duration"].get<int>();
        }
        if (j.contains("qosProfile")) {
            info.qos_profile = j["qosProfile"].get<std::string>();
        }
        if (j.contains("startedAt")) {
            info.started_at = j["startedAt"].get<std::string>();
        }
        if (j.contains("expiresAt")) {
            info.expires_at = j["expiresAt"].get<std::string>();
        }
    } catch (const json::exception& e) {
        spdlog::warn("[QodClient] Failed to parse session JSON: {}", e.what());
    }
    return info;
}

std::vector<SessionInfo> QodClient::parse_session_list(
    const std::string& raw) {
    std::vector<SessionInfo> sessions;
    if (raw.empty()) return sessions;

    try {
        auto arr = json::parse(raw);
        if (!arr.is_array()) {
            // Single object — wrap
            sessions.push_back(parse_session_info(raw));
            return sessions;
        }
        for (const auto& item : arr) {
            sessions.push_back(parse_session_info(item.dump()));
        }
    } catch (const json::exception& e) {
        spdlog::warn("[QodClient] Failed to parse session list: {}", e.what());
    }
    return sessions;
}

}  // namespace phine::adapter
