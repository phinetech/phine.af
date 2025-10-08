/**
 * @brief Helper functions for PCF interactions
 * 
 * Provides utility functions for validating and transforming data
 * related to the Policy Control Function (PCF) interactions.
 */

#pragma once

#include <string>
#include <regex>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace af {
namespace southbound {

// Forward declarations for validation result structure
struct ValidationResult {
    bool is_valid;
    std::vector<std::string> errors;
    
    ValidationResult(bool valid = true) : is_valid(valid) {}
    
    void add_error(const std::string& error) {
        is_valid = false;
        errors.push_back(error);
    }
    
    void merge(const ValidationResult& other) {
        if (!other.is_valid) {
            is_valid = false;
            errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        }
    }
};

/**
 * @brief Validate if a string is a valid URL
 * @param url The URL string to validate
 * @return True if valid, false otherwise
 */
bool is_valid_url(const std::string& url);

/**
 * @brief Validate if a string is a valid UUID
 * @param uuid The UUID string to validate
 * @return True if valid, false otherwise
 */
bool is_valid_uuid(const std::string& uuid);

/**
 * @brief Validate if a string is a valid IPv4 address
 * @param ipv4 The IPv4 string to validate
 * @return True if valid, false otherwise
 */
bool is_valid_ipv4(const std::string& ipv4);

/**
 * @brief Validate if a string is a valid IPv6 address
 * @param ipv6 The IPv6 string to validate
 * @return True if valid, false otherwise
 */
bool is_valid_ipv6(const std::string& ipv6);

/**
 * @brief Validate if a string is a valid MAC address (48-bit)
 * @param mac The MAC address string to validate
 * @return True if valid, false otherwise
 */
bool is_valid_mac_addr48(const std::string& mac);

// Enumeration validation functions
bool is_valid_media_type(const std::string& media_type);
bool is_valid_af_event(const std::string& af_event);
bool is_valid_af_notif_method(const std::string& notif_method);
bool is_valid_flow_status(const std::string& flow_status);
bool is_valid_flow_usage(const std::string& flow_usage);
bool is_valid_reservation_priority(const std::string& res_prio);
bool is_valid_sponsoring_status(const std::string& spon_status);
bool is_valid_service_info_status(const std::string& serv_status);
bool is_valid_termination_cause(const std::string& term_cause);
bool is_valid_qos_notif_type(const std::string& qos_notif_type);
bool is_valid_required_access_info(const std::string& req_access_info);

// Main validation functions for complex schemas
ValidationResult validate_app_session_context(const nlohmann::json& json);
ValidationResult validate_app_session_context_req_data(const nlohmann::json& json);
ValidationResult validate_app_session_context_resp_data(const nlohmann::json& json);
ValidationResult validate_app_session_context_update_data_patch(const nlohmann::json& json);
ValidationResult validate_events_subscription_req_data(const nlohmann::json& json);
ValidationResult validate_media_component(const nlohmann::json& json);
ValidationResult validate_media_sub_component(const nlohmann::json& json);
ValidationResult validate_events_notification(const nlohmann::json& json);
ValidationResult validate_af_event_subscription(const nlohmann::json& json);
ValidationResult validate_af_event_notification(const nlohmann::json& json);
ValidationResult validate_termination_info(const nlohmann::json& json);
ValidationResult validate_af_routing_requirement(const nlohmann::json& json);
ValidationResult validate_spatial_validity(const nlohmann::json& json);
ValidationResult validate_temporal_validity(const nlohmann::json& json);
ValidationResult validate_flows(const nlohmann::json& json);
ValidationResult validate_eth_flow_description(const nlohmann::json& json);
ValidationResult validate_pcscf_restoration_request_data(const nlohmann::json& json);
ValidationResult validate_qos_monitoring_information(const nlohmann::json& json);
ValidationResult validate_qos_monitoring_report(const nlohmann::json& json);
ValidationResult validate_ue_identity_info(const nlohmann::json& json);
ValidationResult validate_access_net_charging_identifier(const nlohmann::json& json);
ValidationResult validate_alternative_service_requirements_data(const nlohmann::json& json);

// Helper functions for common validations
ValidationResult validate_required_fields(const nlohmann::json& json, const std::vector<std::string>& required_fields);
ValidationResult validate_one_of_required(const nlohmann::json& json, const std::vector<std::string>& one_of_fields);
ValidationResult validate_any_of_required(const nlohmann::json& json, const std::vector<std::string>& any_of_fields);
ValidationResult validate_array_min_items(const nlohmann::json& json, const std::string& field_name, size_t min_items);
ValidationResult validate_array_max_items(const nlohmann::json& json, const std::string& field_name, size_t max_items);
ValidationResult validate_integer_range(const nlohmann::json& json, const std::string& field_name, int min_val, int max_val);
ValidationResult validate_string_pattern(const nlohmann::json& json, const std::string& field_name, const std::regex& pattern);

} // namespace southbound
} // namespace af