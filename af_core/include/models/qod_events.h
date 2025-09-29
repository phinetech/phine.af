#ifndef AF_CORE_MODEL_QOD_EVENTS_H
#define AF_CORE_MODEL_QOD_EVENTS_H

#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>
#include "qod_session.h"

namespace af {
namespace qod {

/**
 * @brief CloudEvents specification version
 */
constexpr const char* CLOUDEVENTS_SPEC_VERSION = "1.0";

/**
 * @brief Event types for QoD
 */
namespace EventType {
    constexpr const char* QOS_STATUS_CHANGED = "org.camaraproject.quality-on-demand.v1.qos-status-changed";
}

/**
 * @brief Base CloudEvent structure
 * Compliant with CloudEvents specification 1.0
 */
struct CloudEvent {
    std::string id;                                    // Unique event identifier
    std::string source;                                // Event source URI
    std::string spec_version;                          // CloudEvents spec version (1.0)
    std::string type;                                   // Event type
    std::chrono::system_clock::time_point time;        // Event timestamp
    std::optional<std::string> data_content_type;      // Data content type (application/json)
    nlohmann::json data;                               // Event-specific data
    
    /**
     * @brief Convert to JSON representation
     */
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["id"] = id;
        j["source"] = source;
        j["specversion"] = spec_version;
        j["type"] = type;
        
        // Format time as RFC3339
        auto time_t = std::chrono::system_clock::to_time_t(time);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t));
        j["time"] = std::string(buffer);
        
        if (data_content_type) {
            j["datacontenttype"] = *data_content_type;
        }
        
        j["data"] = data;
        return j;
    }
    
    /**
     * @brief Create from JSON representation
     */
    static CloudEvent from_json(const nlohmann::json& j) {
        CloudEvent event;
        event.id = j["id"];
        event.source = j["source"];
        event.spec_version = j["specversion"];
        event.type = j["type"];
        
        // Parse RFC3339 time
        std::string time_str = j["time"];
        std::tm tm = {};
        std::stringstream ss(time_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        event.time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        
        if (j.contains("datacontenttype")) {
            event.data_content_type = j["datacontenttype"];
        }
        
        event.data = j["data"];
        return event;
    }
};

/**
 * @brief QoS Status Changed event data
 */
struct QosStatusChangedData {
    std::string session_id;
    QosStatus qos_status;
    std::optional<StatusInfo> status_info;
    
    /**
     * @brief Convert to JSON representation
     */
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["sessionId"] = session_id;
        j["qosStatus"] = (qos_status == QosStatus::AVAILABLE) ? "AVAILABLE" : "UNAVAILABLE";
        
        if (status_info && *status_info != StatusInfo::NONE) {
            j["statusInfo"] = QodTypeUtils::status_info_to_string(*status_info);
        }
        
        return j;
    }
    
    /**
     * @brief Create from JSON representation
     */
    static QosStatusChangedData from_json(const nlohmann::json& j) {
        QosStatusChangedData data;
        data.session_id = j["sessionId"];
        
        std::string status_str = j["qosStatus"];
        data.qos_status = (status_str == "AVAILABLE") ? QosStatus::AVAILABLE : QosStatus::UNAVAILABLE;
        
        if (j.contains("statusInfo")) {
            data.status_info = QodTypeUtils::string_to_status_info(j["statusInfo"]);
        }
        
        return data;
    }
};

/**
 * @brief Event builder for QoD CloudEvents
 */
class QodEventBuilder {
public:
    /**
     * @brief Create a QOS_STATUS_CHANGED event
     */
    static CloudEvent create_qos_status_changed_event(
        const std::string& session_id,
        QosStatus new_status,
        std::optional<StatusInfo> status_info = std::nullopt,
        const std::string& source_base_url = "https://api.example.com/qod/v1") {
        
        CloudEvent event;
        event.id = generate_event_id();
        event.source = source_base_url + "/sessions/" + session_id;
        event.spec_version = CLOUDEVENTS_SPEC_VERSION;
        event.type = EventType::QOS_STATUS_CHANGED;
        event.time = std::chrono::system_clock::now();
        event.data_content_type = "application/json";
        
        QosStatusChangedData data;
        data.session_id = session_id;
        data.qos_status = new_status;
        data.status_info = status_info;
        
        event.data = data.to_json();
        
        return event;
    }
    
private:
    /**
     * @brief Generate a unique event ID (UUID-like)
     */
    static std::string generate_event_id() {
        // Simple UUID generation for demonstration
        // In production, use a proper UUID library
        std::stringstream ss;
        std::srand(std::chrono::steady_clock::now().time_since_epoch().count());
        
        for (int i = 0; i < 4; ++i) {
            ss << std::hex << (std::rand() % 0xffff);
            if (i < 3) ss << "-";
        }
        
        return ss.str();
    }
};

/**
 * @brief Notification delivery result
 */
struct NotificationDeliveryResult {
    bool success;
    int http_status_code;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Interface for notification delivery
 */
class INotificationDelivery {
public:
    virtual ~INotificationDelivery() = default;
    
    /**
     * @brief Deliver a CloudEvent to the specified sink
     * @param event The CloudEvent to deliver
     * @param sink The target URL
     * @param credential Optional authentication credential
     * @return Delivery result
     */
    virtual NotificationDeliveryResult deliver(
        const CloudEvent& event,
        const std::string& sink,
        const std::optional<SinkCredential>& credential = std::nullopt) = 0;
};

} // namespace qod
} // namespace af

#endif // AF_CORE_MODEL_QOD_EVENTS_H