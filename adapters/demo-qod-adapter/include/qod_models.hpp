#pragma once
/// @file qod_models.hpp
/// @brief Data models for CAMARA Quality-on-Demand API requests/responses.
///
/// These structs mirror the CAMARA QoD API data types used by phine.af.
/// They are self-contained — no dependency on af_common.

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace phine::adapter {

// ─── Device & Network Models ────────────────────────────────────────────────

struct DeviceIpv4Addr {
    std::string public_address;
    int public_port = 0;
};

struct Device {
    std::optional<std::string> phone_number;
    std::optional<std::string> network_access_identifier;
    std::optional<DeviceIpv4Addr> ipv4_address;
};

struct ApplicationServer {
    std::string ipv4_address;  // may include CIDR, e.g. "0.0.0.0/0"
};

struct PortRange {
    int from = 0;
    int to = 0;
};

struct PortsSpec {
    std::vector<PortRange> ranges;
    std::vector<int> ports;
};

// ─── QoS Enums ──────────────────────────────────────────────────────────────

enum class QosStatus {
    REQUESTED,
    AVAILABLE,
    UNAVAILABLE,
};

enum class StatusInfo {
    NONE,
    DURATION_EXPIRED,
    NETWORK_TERMINATED,
    DELETE_REQUESTED,
};

// ─── Request / Response Models ──────────────────────────────────────────────

struct CreateSessionRequest {
    Device device;
    ApplicationServer application_server;
    std::optional<PortsSpec> device_ports;
    std::optional<PortsSpec> application_server_ports;
    std::string qos_profile;
    int duration_seconds = 3600;
    std::optional<std::string> sink;  // notification callback URL
};

struct ExtendSessionRequest {
    int requested_additional_duration = 0;
};

struct SessionInfo {
    std::string session_id;
    QosStatus qos_status = QosStatus::REQUESTED;
    std::optional<StatusInfo> status_info;
    int duration = 0;
    std::string qos_profile;
    std::optional<std::string> started_at;   // ISO-8601
    std::optional<std::string> expires_at;   // ISO-8601
};

// ─── Result Wrapper ─────────────────────────────────────────────────────────

/// Generic result type.  For void results, use Result<std::monostate>.
template <typename T>
struct Result {
    bool success = false;
    T value{};
    std::string error_code;
    std::string error_message;

    static Result ok(T val) {
        return {true, std::move(val), {}, {}};
    }
    static Result error(std::string code, std::string msg) {
        return {false, {}, std::move(code), std::move(msg)};
    }
};

/// Explicit specialisation for void-like results.
template <>
struct Result<void> {
    bool success = false;
    std::string error_code;
    std::string error_message;

    static Result ok() { return {true, {}, {}}; }
    static Result error(std::string code, std::string msg) {
        return {false, std::move(code), std::move(msg)};
    }
};

// ─── String Conversions ─────────────────────────────────────────────────────

inline const char* to_string(QosStatus s) {
    switch (s) {
        case QosStatus::REQUESTED:   return "REQUESTED";
        case QosStatus::AVAILABLE:   return "AVAILABLE";
        case QosStatus::UNAVAILABLE: return "UNAVAILABLE";
    }
    return "UNKNOWN";
}

inline QosStatus qos_status_from_string(const std::string& s) {
    if (s == "AVAILABLE")   return QosStatus::AVAILABLE;
    if (s == "UNAVAILABLE") return QosStatus::UNAVAILABLE;
    return QosStatus::REQUESTED;
}

inline const char* to_string(StatusInfo s) {
    switch (s) {
        case StatusInfo::NONE:               return "NONE";
        case StatusInfo::DURATION_EXPIRED:    return "DURATION_EXPIRED";
        case StatusInfo::NETWORK_TERMINATED:  return "NETWORK_TERMINATED";
        case StatusInfo::DELETE_REQUESTED:    return "DELETE_REQUESTED";
    }
    return "UNKNOWN";
}

inline StatusInfo status_info_from_string(const std::string& s) {
    if (s == "DURATION_EXPIRED")    return StatusInfo::DURATION_EXPIRED;
    if (s == "NETWORK_TERMINATED")  return StatusInfo::NETWORK_TERMINATED;
    if (s == "DELETE_REQUESTED")    return StatusInfo::DELETE_REQUESTED;
    return StatusInfo::NONE;
}

}  // namespace phine::adapter
