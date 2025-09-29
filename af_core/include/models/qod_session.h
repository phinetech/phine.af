#ifndef AF_CORE_MODEL_QOD_SESSION_H
#define AF_CORE_MODEL_QOD_SESSION_H

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <functional>

#include "common.h"

namespace af {
namespace qod {

/**
 * @brief QoS Status for a QoD session
 * @source CAMARA QualityOnDemand v1.1.0
 */
enum class QosStatus {
    REQUESTED,   // QoS has been requested by creating a session
    AVAILABLE,   // The requested QoS has been provided by the network
    UNAVAILABLE  // The requested QoS cannot be provided by the network
};

/**
 * @brief Status info for QoS session state changes
 * @source CAMARA QualityOnDemand v1.1.0
 */
enum class StatusInfo {
    NONE,                // No specific status info
    DURATION_EXPIRED,    // Session terminated due to requested duration expired
    NETWORK_TERMINATED,  // Network terminated the session before the requested duration expired
    DELETE_REQUESTED     // User requested the deletion of the session
};

/**
 * @brief TCP or UDP port number
 */
using Port = uint16_t;

/**
 * @brief Port range specification
 */
struct PortRange {
    Port from;
    Port to;
    
    bool operator==(const PortRange& other) const {
        return from == other.from && to == other.to;
    }
};

/**
 * @brief Specification of TCP or UDP ports
 */
struct PortsSpec {
    std::vector<PortRange> ranges;
    std::vector<Port> ports;
    
    bool is_empty() const {
        return ranges.empty() && ports.empty();
    }
    
    bool operator==(const PortsSpec& other) const {
        return ranges == other.ranges && ports == other.ports;
    }
};

/**
 * @brief Device IPv4 address information
 */
struct DeviceIpv4Addr {
    Ipv4Addr public_address;
    std::optional<Ipv4Addr> private_address;
    std::optional<Port> public_port;
    
    bool operator==(const DeviceIpv4Addr& other) const {
        return public_address == other.public_address &&
               private_address == other.private_address &&
               public_port == other.public_port;
    }
};

/**
 * @brief Device identifier for CAMARA QoD
 * Maps to the CAMARA Device object
 */
struct QodDevice {
    std::optional<std::string> phone_number;              // E.164 format with '+'
    std::optional<std::string> network_access_identifier; // user@domain format
    std::optional<DeviceIpv4Addr> ipv4_address;
    std::optional<Ipv6Addr> ipv6_address;
    
    bool is_empty() const {
        return !phone_number && !network_access_identifier && 
               !ipv4_address && !ipv6_address;
    }
    
    bool operator==(const QodDevice& other) const {
        return phone_number == other.phone_number &&
               network_access_identifier == other.network_access_identifier &&
               ipv4_address == other.ipv4_address &&
               ipv6_address == other.ipv6_address;
    }
};

/**
 * @brief Application server identifier for CAMARA QoD
 */
struct ApplicationServer {
    std::optional<std::string> ipv4_address;  // Can include CIDR notation
    std::optional<std::string> ipv6_address;  // Can include prefix length
    
    bool is_empty() const {
        return !ipv4_address && !ipv6_address;
    }
    
    bool operator==(const ApplicationServer& other) const {
        return ipv4_address == other.ipv4_address &&
               ipv6_address == other.ipv6_address;
    }
};

/**
 * @brief Sink credential for notifications
 */
struct SinkCredential {
    enum class CredentialType {
        PLAIN,
        ACCESSTOKEN,
        REFRESHTOKEN
    };
    
    CredentialType credential_type;
    
    // For ACCESSTOKEN type
    std::optional<std::string> access_token;
    std::optional<std::chrono::system_clock::time_point> access_token_expires_utc;
    std::optional<std::string> access_token_type;  // "bearer"
    
    // For REFRESHTOKEN type (not used in current version)
    std::optional<std::string> refresh_token;
    std::optional<std::string> refresh_token_endpoint;
    
    // For PLAIN type (not used in current version)
    std::optional<std::string> identifier;
    std::optional<std::string> secret;
    
    bool operator==(const SinkCredential& other) const {
        return credential_type == other.credential_type &&
               access_token == other.access_token &&
               access_token_expires_utc == other.access_token_expires_utc &&
               access_token_type == other.access_token_type;
    }
};

/**
 * @brief Complete QoD session information
 * Represents a CAMARA QualityOnDemand session
 */
struct QodSession {
    // Session identification
    std::string session_id;                           // UUID format
    std::string api_consumer_id;                      // ID of the API consumer who created the session
    
    // Device and application information
    std::optional<QodDevice> device;                  // Device identifier(s)
    std::optional<QodDevice> device_response;         // Single device identifier returned in response
    ApplicationServer application_server;              // Application server endpoint
    
    // Port specifications
    std::optional<PortsSpec> device_ports;            // Device-side ports
    std::optional<PortsSpec> application_server_ports; // Server-side ports
    
    // QoS profile and status
    std::string qos_profile;                          // QoS profile name (e.g., "QOS_L", "QOS_E")
    QosStatus qos_status;                             // Current session status
    std::optional<StatusInfo> status_info;            // Additional status information
    
    // Timing information
    std::chrono::seconds duration;                    // Requested or actual duration
    std::optional<std::chrono::system_clock::time_point> started_at;  // When session became AVAILABLE
    std::optional<std::chrono::system_clock::time_point> expires_at;  // When session will/did expire
    std::chrono::system_clock::time_point created_at; // When session was created
    
    // Notification configuration
    std::optional<std::string> sink;                  // Notification endpoint URL
    std::optional<SinkCredential> sink_credential;    // Authentication for notifications
    
    // PCF integration
    std::optional<std::string> pcf_session_id;        // PCF application session ID
    std::optional<std::string> pcf_transaction_id;    // PCF transaction correlation
    
    // Internal state management
    std::optional<Supi> ue_supi;                      // UE SUPI if resolved
    std::optional<std::string> pdu_session_id;        // Associated PDU session if identified
    std::string error_message;                        // Error details if any
    
    bool operator==(const QodSession& other) const {
        return session_id == other.session_id;
    }
};

/**
 * @brief Request to create a QoD session
 */
struct CreateSessionRequest {
    std::optional<QodDevice> device;
    ApplicationServer application_server;
    std::optional<PortsSpec> device_ports;
    std::optional<PortsSpec> application_server_ports;
    std::string qos_profile;
    std::chrono::seconds duration;
    std::optional<std::string> sink;
    std::optional<SinkCredential> sink_credential;
    std::string api_consumer_id;
    std::optional<std::string> correlation_id;
};

/**
 * @brief Request to extend session duration
 */
struct ExtendSessionDurationRequest {
    std::string session_id;
    std::chrono::seconds requested_additional_duration;
};

/**
 * @brief Request to retrieve sessions for a device
 */
struct RetrieveSessionsRequest {
    std::optional<QodDevice> device;
    std::string api_consumer_id;
};

/**
 * @brief Utility functions for QoD types
 */
class QodTypeUtils {
public:
    static std::string qos_status_to_string(QosStatus status) {
        switch (status) {
            case QosStatus::REQUESTED: return "REQUESTED";
            case QosStatus::AVAILABLE: return "AVAILABLE";
            case QosStatus::UNAVAILABLE: return "UNAVAILABLE";
            default: return "UNKNOWN";
        }
    }
    
    static QosStatus string_to_qos_status(const std::string& str) {
        if (str == "REQUESTED") return QosStatus::REQUESTED;
        if (str == "AVAILABLE") return QosStatus::AVAILABLE;
        if (str == "UNAVAILABLE") return QosStatus::UNAVAILABLE;
        return QosStatus::UNAVAILABLE; // Default
    }
    
    static std::string status_info_to_string(StatusInfo info) {
        switch (info) {
            case StatusInfo::DURATION_EXPIRED: return "DURATION_EXPIRED";
            case StatusInfo::NETWORK_TERMINATED: return "NETWORK_TERMINATED";
            case StatusInfo::DELETE_REQUESTED: return "DELETE_REQUESTED";
            case StatusInfo::NONE: return "";
            default: return "";
        }
    }
    
    static StatusInfo string_to_status_info(const std::string& str) {
        if (str == "DURATION_EXPIRED") return StatusInfo::DURATION_EXPIRED;
        if (str == "NETWORK_TERMINATED") return StatusInfo::NETWORK_TERMINATED;
        if (str == "DELETE_REQUESTED") return StatusInfo::DELETE_REQUESTED;
        return StatusInfo::NONE;
    }
};

} // namespace qod
} // namespace af

// Hash specializations for QoD types
namespace std {

template<> struct hash<af::qod::PortRange> {
    size_t operator()(const af::qod::PortRange& pr) const {
        size_t h = hash<uint16_t>{}(pr.from);
        hash_combine(h, hash<uint16_t>{}(pr.to));
        return h;
    }
};

template<> struct hash<af::qod::PortsSpec> {
    size_t operator()(const af::qod::PortsSpec& ps) const {
        size_t h = 0;
        for (const auto& range : ps.ranges) {
            hash_combine(h, hash<af::qod::PortRange>{}(range));
        }
        for (const auto& port : ps.ports) {
            hash_combine(h, hash<uint16_t>{}(port));
        }
        return h;
    }
};

template<> struct hash<af::qod::DeviceIpv4Addr> {
    size_t operator()(const af::qod::DeviceIpv4Addr& d) const {
        size_t h = hash<Ipv4Addr>{}(d.public_address);
        hash_combine(h, hash_optional(d.private_address));
        hash_combine(h, hash_optional(d.public_port));
        return h;
    }
};

template<> struct hash<af::qod::QodDevice> {
    size_t operator()(const af::qod::QodDevice& d) const {
        size_t h = hash_optional(d.phone_number);
        hash_combine(h, hash_optional(d.network_access_identifier));
        hash_combine(h, hash_optional(d.ipv4_address));
        hash_combine(h, hash_optional(d.ipv6_address));
        return h;
    }
};

template<> struct hash<af::qod::ApplicationServer> {
    size_t operator()(const af::qod::ApplicationServer& as) const {
        size_t h = hash_optional(as.ipv4_address);
        hash_combine(h, hash_optional(as.ipv6_address));
        return h;
    }
};

template<> struct hash<af::qod::QodSession> {
    size_t operator()(const af::qod::QodSession& qs) const {
        return hash<string>{}(qs.session_id);
    }
};

} // namespace std

#endif // AF_CORE_MODEL_QOD_SESSION_H