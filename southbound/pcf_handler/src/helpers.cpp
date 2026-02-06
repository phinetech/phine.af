/**
 * @brief Implementation of helper functions for PCF interactions
 * 
 * Provides implementation of utility functions for validating and transforming data
 * related to the Policy Control Function (PCF) interactions.
 */

#include "helpers.h"
#include <regex>
#include <set>
#include <algorithm>

namespace af {
namespace southbound {

// Basic validation functions
bool is_valid_url(const std::string& url) {
    const std::regex url_regex(
        R"(^(https?|ftp)://[^\s/$.?#].[^\s]*$)",
        std::regex::icase);
    return std::regex_match(url, url_regex);
}

bool is_valid_uuid(const std::string& uuid) {
    const std::regex uuid_regex(
        R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)",
        std::regex::icase);
    return std::regex_match(uuid, uuid_regex);
}

bool is_valid_ipv4(const std::string& ipv4) {
    const std::regex ipv4_regex(
        R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    return std::regex_match(ipv4, ipv4_regex);
}

bool is_valid_ipv6(const std::string& ipv6) {
    const std::regex ipv6_regex(
        R"(^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::1$|^::$|^(?:[0-9a-fA-F]{1,4}:)*::[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4})*$)");
    return std::regex_match(ipv6, ipv6_regex);
}

bool is_valid_mac_addr48(const std::string& mac) {
    const std::regex mac_regex(
        R"(^[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}$)");
    return std::regex_match(mac, mac_regex);
}

// Enumeration validation functions
bool is_valid_media_type(const std::string& media_type) {
    static const std::set<std::string> valid_types = {
        "AUDIO", "VIDEO", "DATA", "APPLICATION", "CONTROL", "TEXT", "MESSAGE", "OTHER"
    };
    return valid_types.find(media_type) != valid_types.end();
}

bool is_valid_af_event(const std::string& af_event) {
    static const std::set<std::string> valid_events = {
        "ACCESS_TYPE_CHANGE", "EXTRA_UE_ADDR", "ANI_REPORT", "APP_DETECTION",
        "CHARGING_CORRELATION", "EPS_FALLBACK", "FAILED_QOS_UPDATE", "FAILED_RESOURCES_ALLOCATION",
        "OUT_OF_CREDIT", "PDU_SESSION_STATUS", "PLMN_CHG", "QOS_MONITORING", "QOS_NOTIF",
        "RAN_NAS_CAUSE", "REALLOCATION_OF_CREDIT", "SAT_CATEGORY_CHG", "SUCCESSFUL_QOS_UPDATE",
        "SUCCESSFUL_RESOURCES_ALLOCATION", "TSN_BRIDGE_INFO", "UP_PATH_CHG_FAILURE",
        "USAGE_REPORT", "UE_TEMPORARILY_UNAVAILABLE"
    };
    return valid_events.find(af_event) != valid_events.end();
}

bool is_valid_af_notif_method(const std::string& notif_method) {
    static const std::set<std::string> valid_methods = {
        "EVENT_DETECTION", "ONE_TIME", "PERIODIC", "PDU_SESSION_RELEASE"
    };
    return valid_methods.find(notif_method) != valid_methods.end();
}

bool is_valid_flow_status(const std::string& flow_status) {
    static const std::set<std::string> valid_statuses = {
        "ENABLED-UPLINK", "ENABLED-DOWNLINK", "ENABLED", "DISABLED", "REMOVED"
    };
    return valid_statuses.find(flow_status) != valid_statuses.end();
}

bool is_valid_flow_usage(const std::string& flow_usage) {
    static const std::set<std::string> valid_usages = {
        "NO_INFO", "RTCP", "AF_SIGNALLING"
    };
    return valid_usages.find(flow_usage) != valid_usages.end();
}

bool is_valid_reservation_priority(const std::string& res_prio) {
    static const std::set<std::string> valid_priorities = {
        "PRIO_1", "PRIO_2", "PRIO_3", "PRIO_4", "PRIO_5", "PRIO_6", "PRIO_7", "PRIO_8",
        "PRIO_9", "PRIO_10", "PRIO_11", "PRIO_12", "PRIO_13", "PRIO_14", "PRIO_15", "PRIO_16"
    };
    return valid_priorities.find(res_prio) != valid_priorities.end();
}

bool is_valid_sponsoring_status(const std::string& spon_status) {
    static const std::set<std::string> valid_statuses = {
        "SPONSOR_DISABLED", "SPONSOR_ENABLED"
    };
    return valid_statuses.find(spon_status) != valid_statuses.end();
}

bool is_valid_service_info_status(const std::string& serv_status) {
    static const std::set<std::string> valid_statuses = {
        "FINAL", "PRELIMINARY"
    };
    return valid_statuses.find(serv_status) != valid_statuses.end();
}

bool is_valid_termination_cause(const std::string& term_cause) {
    static const std::set<std::string> valid_causes = {
        "ALL_SDF_DEACTIVATION", "PDU_SESSION_TERMINATION", "PS_TO_CS_HO",
        "INSUFFICIENT_SERVER_RESOURCES", "INSUFFICIENT_QOS_FLOW_RESOURCES",
        "SPONSORED_DATA_CONNECTIVITY_DISALLOWED"
    };
    return valid_causes.find(term_cause) != valid_causes.end();
}

bool is_valid_qos_notif_type(const std::string& qos_notif_type) {
    static const std::set<std::string> valid_types = {
        "GUARANTEED", "NOT_GUARANTEED"
    };
    return valid_types.find(qos_notif_type) != valid_types.end();
}

bool is_valid_required_access_info(const std::string& req_access_info) {
    static const std::set<std::string> valid_info = {
        "USER_LOCATION", "UE_TIME_ZONE"
    };
    return valid_info.find(req_access_info) != valid_info.end();
}

// Helper validation functions
ValidationResult validate_required_fields(const nlohmann::json& json, const std::vector<std::string>& required_fields) {
    ValidationResult result;
    
    for (const auto& field : required_fields) {
        if (!json.contains(field)) {
            result.add_error("Missing required field: " + field);
        }
    }
    
    return result;
}

ValidationResult validate_one_of_required(const nlohmann::json& json, const std::vector<std::string>& one_of_fields) {
    ValidationResult result;
    
    int found_count = 0;
    for (const auto& field : one_of_fields) {
        if (json.contains(field)) {
            found_count++;
        }
    }
    
    if (found_count != 1) {
        result.add_error("Exactly one of the following fields must be present: " + 
                        std::accumulate(one_of_fields.begin(), one_of_fields.end(), std::string(),
                                      [](const std::string& a, const std::string& b) {
                                          return a.empty() ? b : a + ", " + b;
                                      }));
    }
    
    return result;
}

ValidationResult validate_any_of_required(const nlohmann::json& json, const std::vector<std::string>& any_of_fields) {
    ValidationResult result;
    
    bool found = false;
    for (const auto& field : any_of_fields) {
        if (json.contains(field)) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        result.add_error("At least one of the following fields must be present: " + 
                        std::accumulate(any_of_fields.begin(), any_of_fields.end(), std::string(),
                                      [](const std::string& a, const std::string& b) {
                                          return a.empty() ? b : a + ", " + b;
                                      }));
    }
    
    return result;
}

ValidationResult validate_array_min_items(const nlohmann::json& json, const std::string& field_name, size_t min_items) {
    ValidationResult result;
    
    if (json.contains(field_name)) {
        if (!json[field_name].is_array()) {
            result.add_error("Field " + field_name + " must be an array");
        } else if (json[field_name].size() < min_items) {
            result.add_error("Field " + field_name + " must have at least " + std::to_string(min_items) + " items");
        }
    }
    
    return result;
}

ValidationResult validate_array_max_items(const nlohmann::json& json, const std::string& field_name, size_t max_items) {
    ValidationResult result;
    
    if (json.contains(field_name)) {
        if (!json[field_name].is_array()) {
            result.add_error("Field " + field_name + " must be an array");
        } else if (json[field_name].size() > max_items) {
            result.add_error("Field " + field_name + " must have at most " + std::to_string(max_items) + " items");
        }
    }
    
    return result;
}

ValidationResult validate_integer_range(const nlohmann::json& json, const std::string& field_name, int min_val, int max_val) {
    ValidationResult result;
    
    if (json.contains(field_name)) {
        if (!json[field_name].is_number_integer()) {
            result.add_error("Field " + field_name + " must be an integer");
        } else {
            int value = json[field_name];
            if (value < min_val || value > max_val) {
                result.add_error("Field " + field_name + " must be between " + std::to_string(min_val) + 
                               " and " + std::to_string(max_val));
            }
        }
    }
    
    return result;
}

ValidationResult validate_string_pattern(const nlohmann::json& json, const std::string& field_name, const std::regex& pattern) {
    ValidationResult result;
    
    if (json.contains(field_name)) {
        if (!json[field_name].is_string()) {
            result.add_error("Field " + field_name + " must be a string");
        } else {
            std::string value = json[field_name];
            if (!std::regex_match(value, pattern)) {
                result.add_error("Field " + field_name + " does not match required pattern");
            }
        }
    }
    
    return result;
}

// Main validation functions for complex schemas

ValidationResult validate_app_session_context(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AppSessionContext must be an object");
        return result;
    }
    
    // Validate optional fields if present
    if (json.contains("ascReqData")) {
        result.merge(validate_app_session_context_req_data(json["ascReqData"]));
    }
    
    if (json.contains("ascRespData")) {
        result.merge(validate_app_session_context_resp_data(json["ascRespData"]));
    }
    
    if (json.contains("evsNotif")) {
        result.merge(validate_events_notification(json["evsNotif"]));
    }
    
    return result;
}

ValidationResult validate_app_session_context_req_data(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AppSessionContextReqData must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"notifUri", "suppFeat"}));
    
    // Check oneOf requirement for UE identification
    result.merge(validate_one_of_required(json, {"ueIpv4", "ueIpv6", "ueMac"}));
    
    // Validate URI format
    if (json.contains("notifUri") && json["notifUri"].is_string()) {
        std::string uri = json["notifUri"];
        if (!is_valid_url(uri)) {
            result.add_error("notifUri must be a valid URI");
        }
    }
    
    // Validate IP addresses if present
    if (json.contains("ueIpv4") && json["ueIpv4"].is_string()) {
        std::string ipv4 = json["ueIpv4"];
        if (!is_valid_ipv4(ipv4)) {
            result.add_error("ueIpv4 must be a valid IPv4 address");
        }
    }
    
    if (json.contains("ueIpv6") && json["ueIpv6"].is_string()) {
        std::string ipv6 = json["ueIpv6"];
        if (!is_valid_ipv6(ipv6)) {
            result.add_error("ueIpv6 must be a valid IPv6 address");
        }
    }
    
    if (json.contains("ueMac") && json["ueMac"].is_string()) {
        std::string mac = json["ueMac"];
        if (!is_valid_mac_addr48(mac)) {
            result.add_error("ueMac must be a valid MAC address");
        }
    }
    
    // Validate enumeration fields
    if (json.contains("resPrio") && json["resPrio"].is_string()) {
        if (!is_valid_reservation_priority(json["resPrio"])) {
            result.add_error("resPrio contains invalid reservation priority value");
        }
    }
    
    if (json.contains("sponStatus") && json["sponStatus"].is_string()) {
        if (!is_valid_sponsoring_status(json["sponStatus"])) {
            result.add_error("sponStatus contains invalid sponsoring status value");
        }
    }
    
    if (json.contains("servInfStatus") && json["servInfStatus"].is_string()) {
        if (!is_valid_service_info_status(json["servInfStatus"])) {
            result.add_error("servInfStatus contains invalid service info status value");
        }
    }
    
    // Validate nested objects
    if (json.contains("evSubsc")) {
        result.merge(validate_events_subscription_req_data(json["evSubsc"]));
    }
    
    if (json.contains("medComponents")) {
        if(!json["medComponents"].is_object()) {
            result.add_error("medComponents must be an object");
            return result;
        } else {
            for (const auto& [key, component] : json["medComponents"].items()) {
                result.merge(validate_media_component(component));
            }
        }
    }
    
    if (json.contains("afRoutReq")) {
        result.merge(validate_af_routing_requirement(json["afRoutReq"]));
    }
    
    // Validate array constraints
    if (json.contains("tsnPortManContNwtts")) {
        result.merge(validate_array_min_items(json, "tsnPortManContNwtts", 1));
    }
    
    return result;
}

ValidationResult validate_app_session_context_resp_data(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AppSessionContextRespData must be an object");
        return result;
    }
    
    // Validate UE identities array if present
    if (json.contains("ueIds")) {
        result.merge(validate_array_min_items(json, "ueIds", 1));
        
        if (json["ueIds"].is_array()) {
            for (const auto& ue_id : json["ueIds"]) {
                result.merge(validate_ue_identity_info(ue_id));
            }
        }
    }
    
    return result;
}

ValidationResult validate_events_subscription_req_data(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("EventsSubscReqData must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"events"}));
    
    // Validate events array
    result.merge(validate_array_min_items(json, "events", 1));
    
    if (json.contains("events") && json["events"].is_array()) {
        for (const auto& event : json["events"]) {
            result.merge(validate_af_event_subscription(event));
        }
    }
    
    // Validate optional fields
    if (json.contains("notifUri") && json["notifUri"].is_string()) {
        std::string uri = json["notifUri"];
        if (!is_valid_url(uri)) {
            result.add_error("notifUri must be a valid URI");
        }
    }
    
    // Validate array constraints
    if (json.contains("reqQosMonParams")) {
        result.merge(validate_array_min_items(json, "reqQosMonParams", 1));
    }
    
    if (json.contains("reqAnis")) {
        result.merge(validate_array_min_items(json, "reqAnis", 1));
        
        if (json["reqAnis"].is_array()) {
            for (const auto& ani : json["reqAnis"]) {
                if (ani.is_string() && !is_valid_required_access_info(ani)) {
                    result.add_error("reqAnis contains invalid access info value: " + std::string(ani));
                }
            }
        }
    }
    
    if (json.contains("afAppIds")) {
        result.merge(validate_array_min_items(json, "afAppIds", 1));
    }
    
    if (json.contains("qosMon")) {
        result.merge(validate_qos_monitoring_information(json["qosMon"]));
    }
    
    return result;
}

ValidationResult validate_media_component(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("MediaComponent must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"medCompN"}));
    
    // Validate medCompN is integer
    if (json.contains("medCompN") && !json["medCompN"].is_number_integer()) {
        result.add_error("medCompN must be an integer");
    }
    
    // Validate enumeration fields
    if (json.contains("medType") && json["medType"].is_string()) {
        if (!is_valid_media_type(json["medType"])) {
            result.add_error("medType contains invalid media type value");
        }
    }
    
    if (json.contains("fStatus") && json["fStatus"].is_string()) {
        if (!is_valid_flow_status(json["fStatus"])) {
            result.add_error("fStatus contains invalid flow status value");
        }
    }
    
    if (json.contains("resPrio") && json["resPrio"].is_string()) {
        if (!is_valid_reservation_priority(json["resPrio"])) {
            result.add_error("resPrio contains invalid reservation priority value");
        }
    }
    
    // Validate array constraints
    if (json.contains("codecs")) {
        result.merge(validate_array_min_items(json, "codecs", 1));
        result.merge(validate_array_max_items(json, "codecs", 2));
    }
    
    if (json.contains("altSerReqs")) {
        result.merge(validate_array_min_items(json, "altSerReqs", 1));
    }
    
    if (json.contains("altSerReqsData")) {
        result.merge(validate_array_min_items(json, "altSerReqsData", 1));
        
        if (json["altSerReqsData"].is_array()) {
            for (const auto& req_data : json["altSerReqsData"]) {
                result.merge(validate_alternative_service_requirements_data(req_data));
            }
        }
    }
    
    // Validate nested media subcomponents
    if (json.contains("medSubComps")) {
        if (!json["medSubComps"].is_object()) {
            result.add_error("medSubComps must be an object");
            return result;
        }
        if (json["medSubComps"].empty()) {
            result.add_error("medSubComps must have at least one property");
        }
        
        for (const auto& [key, subcomp] : json["medSubComps"].items()) {
            result.merge(validate_media_sub_component(subcomp));
        }
    }
    
    // Validate constraints - cannot have both altSerReqs and altSerReqsData
    if (json.contains("altSerReqs") && json.contains("altSerReqsData")) {
        result.add_error("Cannot have both altSerReqs and altSerReqsData");
    }
    
    // Validate constraints - cannot have both qosReference and altSerReqsData
    if (json.contains("qosReference") && json.contains("altSerReqsData")) {
        result.add_error("Cannot have both qosReference and altSerReqsData");
    }
    
    return result;
}

ValidationResult validate_media_sub_component(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("MediaSubComponent must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"fNum"}));
    
    // Validate fNum is integer
    if (json.contains("fNum") && !json["fNum"].is_number_integer()) {
        result.add_error("fNum must be an integer");
    }
    
    // Validate array constraints
    if (json.contains("ethfDescs")) {
        result.merge(validate_array_min_items(json, "ethfDescs", 1));
        result.merge(validate_array_max_items(json, "ethfDescs", 2));
        
        if (json["ethfDescs"].is_array()) {
            for (const auto& eth_desc : json["ethfDescs"]) {
                result.merge(validate_eth_flow_description(eth_desc));
            }
        }
    }
    
    if (json.contains("fDescs")) {
        result.merge(validate_array_min_items(json, "fDescs", 1));
        result.merge(validate_array_max_items(json, "fDescs", 2));
    }
    
    // Validate enumeration fields
    if (json.contains("fStatus") && json["fStatus"].is_string()) {
        if (!is_valid_flow_status(json["fStatus"])) {
            result.add_error("fStatus contains invalid flow status value");
        }
    }
    
    if (json.contains("flowUsage") && json["flowUsage"].is_string()) {
        if (!is_valid_flow_usage(json["flowUsage"])) {
            result.add_error("flowUsage contains invalid flow usage value");
        }
    }
    
    return result;
}

ValidationResult validate_af_event_subscription(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AfEventSubscription must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"event"}));
    
    // Validate event enumeration
    if (json.contains("event") && json["event"].is_string()) {
        if (!is_valid_af_event(json["event"])) {
            result.add_error("event contains invalid AF event value");
        }
    }
    
    // Validate notification method enumeration
    if (json.contains("notifMethod") && json["notifMethod"].is_string()) {
        if (!is_valid_af_notif_method(json["notifMethod"])) {
            result.add_error("notifMethod contains invalid notification method value");
        }
    }
    
    return result;
}

ValidationResult validate_events_notification(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("EventsNotification must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"evSubsUri", "evNotifs"}));
    
    // Validate URI format
    if (json.contains("evSubsUri") && json["evSubsUri"].is_string()) {
        std::string uri = json["evSubsUri"];
        if (!is_valid_url(uri)) {
            result.add_error("evSubsUri must be a valid URI");
        }
    }
    
    // Validate evNotifs array
    result.merge(validate_array_min_items(json, "evNotifs", 1));
    
    if (json.contains("evNotifs") && json["evNotifs"].is_array()) {
        for (const auto& notif : json["evNotifs"]) {
            result.merge(validate_af_event_notification(notif));
        }
    }
    
    // Validate other arrays with minimum items
    if (json.contains("adReports")) {
        result.merge(validate_array_min_items(json, "adReports", 1));
    }
    
    if (json.contains("anChargIds")) {
        result.merge(validate_array_min_items(json, "anChargIds", 1));
        
        if (json["anChargIds"].is_array()) {
            for (const auto& charging_id : json["anChargIds"]) {
                result.merge(validate_access_net_charging_identifier(charging_id));
            }
        }
    }
    
    if (json.contains("failedResourcAllocReports")) {
        result.merge(validate_array_min_items(json, "failedResourcAllocReports", 1));
    }
    
    if (json.contains("succResourcAllocReports")) {
        result.merge(validate_array_min_items(json, "succResourcAllocReports", 1));
    }
    
    if (json.contains("qosMonReports")) {
        result.merge(validate_array_min_items(json, "qosMonReports", 1));
        
        if (json["qosMonReports"].is_array()) {
            for (const auto& report : json["qosMonReports"]) {
                result.merge(validate_qos_monitoring_report(report));
            }
        }
    }
    
    return result;
}

ValidationResult validate_af_event_notification(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AfEventNotification must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"event"}));
    
    // Validate event enumeration
    if (json.contains("event") && json["event"].is_string()) {
        if (!is_valid_af_event(json["event"])) {
            result.add_error("event contains invalid AF event value");
        }
    }
    
    // Validate flows array
    if (json.contains("flows")) {
        result.merge(validate_array_min_items(json, "flows", 1));
        
        if (json["flows"].is_array()) {
            for (const auto& flow : json["flows"]) {
                result.merge(validate_flows(flow));
            }
        }
    }
    
    return result;
}

ValidationResult validate_termination_info(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("TerminationInfo must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"termCause", "resUri"}));
    
    // Validate termination cause enumeration
    if (json.contains("termCause") && json["termCause"].is_string()) {
        if (!is_valid_termination_cause(json["termCause"])) {
            result.add_error("termCause contains invalid termination cause value");
        }
    }
    
    // Validate URI format
    if (json.contains("resUri") && json["resUri"].is_string()) {
        std::string uri = json["resUri"];
        if (!is_valid_url(uri)) {
            result.add_error("resUri must be a valid URI");
        }
    }
    
    return result;
}

ValidationResult validate_flows(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("Flows must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"medCompN"}));
    
    // Validate medCompN is integer
    if (json.contains("medCompN") && !json["medCompN"].is_number_integer()) {
        result.add_error("medCompN must be an integer");
    }
    
    // Validate arrays with minimum items
    if (json.contains("contVers")) {
        result.merge(validate_array_min_items(json, "contVers", 1));
    }
    
    if (json.contains("fNums")) {
        result.merge(validate_array_min_items(json, "fNums", 1));
        
        // Validate that all fNums are integers
        if (json["fNums"].is_array()) {
            for (const auto& fnum : json["fNums"]) {
                if (!fnum.is_number_integer()) {
                    result.add_error("All fNums must be integers");
                    break;
                }
            }
        }
    }
    
    return result;
}

ValidationResult validate_eth_flow_description(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("EthFlowDescription must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"ethType"}));
    
    // Validate MAC addresses if present
    if (json.contains("destMacAddr") && json["destMacAddr"].is_string()) {
        if (!is_valid_mac_addr48(json["destMacAddr"])) {
            result.add_error("destMacAddr must be a valid MAC address");
        }
    }
    
    if (json.contains("sourceMacAddr") && json["sourceMacAddr"].is_string()) {
        if (!is_valid_mac_addr48(json["sourceMacAddr"])) {
            result.add_error("sourceMacAddr must be a valid MAC address");
        }
    }
    
    if (json.contains("srcMacAddrEnd") && json["srcMacAddrEnd"].is_string()) {
        if (!is_valid_mac_addr48(json["srcMacAddrEnd"])) {
            result.add_error("srcMacAddrEnd must be a valid MAC address");
        }
    }
    
    if (json.contains("destMacAddrEnd") && json["destMacAddrEnd"].is_string()) {
        if (!is_valid_mac_addr48(json["destMacAddrEnd"])) {
            result.add_error("destMacAddrEnd must be a valid MAC address");
        }
    }
    
    // Validate VLAN tags array constraints
    if (json.contains("vlanTags")) {
        result.merge(validate_array_min_items(json, "vlanTags", 1));
        result.merge(validate_array_max_items(json, "vlanTags", 2));
    }
    
    return result;
}

ValidationResult validate_pcscf_restoration_request_data(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("PcscfRestorationRequestData must be an object");
        return result;
    }
    
    // Check oneOf requirement for UE IP
    result.merge(validate_one_of_required(json, {"ueIpv4", "ueIpv6"}));
    
    // Validate IP addresses
    if (json.contains("ueIpv4") && json["ueIpv4"].is_string()) {
        if (!is_valid_ipv4(json["ueIpv4"])) {
            result.add_error("ueIpv4 must be a valid IPv4 address");
        }
    }
    
    if (json.contains("ueIpv6") && json["ueIpv6"].is_string()) {
        if (!is_valid_ipv6(json["ueIpv6"])) {
            result.add_error("ueIpv6 must be a valid IPv6 address");
        }
    }
    
    return result;
}

ValidationResult validate_qos_monitoring_information(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("QosMonitoringInformation must be an object");
        return result;
    }
    
    // Validate integer fields
    if (json.contains("repThreshDl") && !json["repThreshDl"].is_number_integer()) {
        result.add_error("repThreshDl must be an integer");
    }
    
    if (json.contains("repThreshUl") && !json["repThreshUl"].is_number_integer()) {
        result.add_error("repThreshUl must be an integer");
    }
    
    if (json.contains("repThreshRp") && !json["repThreshRp"].is_number_integer()) {
        result.add_error("repThreshRp must be an integer");
    }
    
    return result;
}

ValidationResult validate_qos_monitoring_report(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("QosMonitoringReport must be an object");
        return result;
    }
    
    // Validate flows array
    if (json.contains("flows")) {
        result.merge(validate_array_min_items(json, "flows", 1));
        
        if (json["flows"].is_array()) {
            for (const auto& flow : json["flows"]) {
                result.merge(validate_flows(flow));
            }
        }
    }
    
    // Validate delay arrays
    if (json.contains("ulDelays")) {
        result.merge(validate_array_min_items(json, "ulDelays", 1));
        
        if (json["ulDelays"].is_array()) {
            for (const auto& delay : json["ulDelays"]) {
                if (!delay.is_number_integer()) {
                    result.add_error("All ulDelays must be integers");
                    break;
                }
            }
        }
    }
    
    if (json.contains("dlDelays")) {
        result.merge(validate_array_min_items(json, "dlDelays", 1));
        
        if (json["dlDelays"].is_array()) {
            for (const auto& delay : json["dlDelays"]) {
                if (!delay.is_number_integer()) {
                    result.add_error("All dlDelays must be integers");
                    break;
                }
            }
        }
    }
    
    if (json.contains("rtDelays")) {
        result.merge(validate_array_min_items(json, "rtDelays", 1));
        
        if (json["rtDelays"].is_array()) {
            for (const auto& delay : json["rtDelays"]) {
                if (!delay.is_number_integer()) {
                    result.add_error("All rtDelays must be integers");
                    break;
                }
            }
        }
    }
    
    return result;
}

ValidationResult validate_ue_identity_info(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("UeIdentityInfo must be an object");
        return result;
    }
    
    // Check anyOf requirement
    result.merge(validate_any_of_required(json, {"gpsi", "pei", "supi"}));
    
    return result;
}

ValidationResult validate_access_net_charging_identifier(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AccessNetChargingIdentifier must be an object");
        return result;
    }
    
    // Check oneOf requirement
    result.merge(validate_one_of_required(json, {"accNetChaIdValue", "accNetChargIdString"}));
    
    // Validate flows array
    if (json.contains("flows")) {
        result.merge(validate_array_min_items(json, "flows", 1));
        
        if (json["flows"].is_array()) {
            for (const auto& flow : json["flows"]) {
                result.merge(validate_flows(flow));
            }
        }
    }
    
    return result;
}

ValidationResult validate_alternative_service_requirements_data(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AlternativeServiceRequirementsData must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"altQosParamSetRef"}));
    
    return result;
}

ValidationResult validate_af_routing_requirement(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AfRoutingRequirement must be an object");
        return result;
    }
    
    // Validate arrays with minimum items
    if (json.contains("routeToLocs")) {
        result.merge(validate_array_min_items(json, "routeToLocs", 1));
    }
    
    if (json.contains("tempVals")) {
        result.merge(validate_array_min_items(json, "tempVals", 1));
        
        if (json["tempVals"].is_array()) {
            for (const auto& temp_val : json["tempVals"]) {
                result.merge(validate_temporal_validity(temp_val));
            }
        }
    }
    
    if (json.contains("easIpReplaceInfos")) {
        result.merge(validate_array_min_items(json, "easIpReplaceInfos", 1));
    }
    
    // Validate spatial validity
    if (json.contains("spVal")) {
        result.merge(validate_spatial_validity(json["spVal"]));
    }
    
    return result;
}

ValidationResult validate_spatial_validity(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("SpatialValidity must be an object");
        return result;
    }
    
    // Check required fields
    result.merge(validate_required_fields(json, {"presenceInfoList"}));
    
    // Validate presenceInfoList is an object with at least one property
    if (json.contains("presenceInfoList")) {
        if (!json["presenceInfoList"].is_object()) {
            result.add_error("presenceInfoList must be an object");
        } else if (json["presenceInfoList"].empty()) {
            result.add_error("presenceInfoList must have at least one property");
        }
    }
    
    return result;
}

ValidationResult validate_temporal_validity(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("TemporalValidity must be an object");
        return result;
    }
    
    // DateTime validation could be added here if needed
    // Currently just checking if the fields are strings
    if (json.contains("startTime") && !json["startTime"].is_string()) {
        result.add_error("startTime must be a string");
    }
    
    if (json.contains("stopTime") && !json["stopTime"].is_string()) {
        result.add_error("stopTime must be a string");
    }
    
    return result;
}

ValidationResult validate_app_session_context_update_data_patch(const nlohmann::json& json) {
    ValidationResult result;
    
    if (!json.is_object()) {
        result.add_error("AppSessionContextUpdateDataPatch must be an object");
        return result;
    }
    
    // Validate ascReqData if present (it should contain AppSessionContextUpdateData)
    if (json.contains("ascReqData")) {
        // This would validate the update data structure which is similar to req data
        // but allows null values for certain fields
        if (!json["ascReqData"].is_object()) {
            result.add_error("ascReqData must be an object");
        }
        // Additional validation for update-specific rules could be added here
    }
    
    return result;
}

} // namespace southbound
} // namespace af