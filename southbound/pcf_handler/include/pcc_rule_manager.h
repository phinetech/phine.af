/**
 * @file pcc_rule_manager.h
 * @brief Manager for PCC rules
 *
 * This component manages PCC (Policy and Charging Control) rules for the AF.
 * It provides functionality to create, validate, and process PCC rules for
 * quality of service and charging policies.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace af {
namespace southbound {

/**
 * @brief Structure representing a PCC rule
 */
struct PccRule {
    std::string rule_id;
    std::string flow_direction;  // uplink, downlink, bidirectional
    std::vector<std::string> flow_descriptions;
    int qos_reference;           // 5QI value
    int guaranteed_bandwidth_ul; // in kbps
    int guaranteed_bandwidth_dl; // in kbps
    int maximum_bandwidth_ul;    // in kbps
    int maximum_bandwidth_dl;    // in kbps
    std::unordered_map<std::string, std::string> additional_params;
};

/**
 * @brief Manager for PCC rules
 */
class PccRuleManager {
public:
    /**
     * @brief Constructor
     */
    PccRuleManager();
    
    /**
     * @brief Destructor
     */
    ~PccRuleManager();
    
    /**
     * @brief Initialize the PCC rule manager
     */
    void initialize();
    
    /**
     * @brief Create a PCC rule for a media component
     * @param media_component Media component information
     * @return PCC rule or nullptr if creation failed
     */
    std::shared_ptr<PccRule> create_rule_from_media_component(
        const nlohmann::json& media_component);
    
    /**
     * @brief Create a PCC rule from application session data
     * @param app_session_data Application session data
     * @return Created PCC rule or nullptr if creation failed
     */
    std::shared_ptr<PccRule> create_rule_from_app_session(
        const nlohmann::json& app_session_data);
    
    /**
     * @brief Convert a PCC rule to PCF API format
     * @param rule PCC rule to convert
     * @return JSON representation of the rule
     */
    nlohmann::json convert_rule_to_pcf_format(const PccRule& rule);
    
    /**
     * @brief Parse a PCC rule from PCF API format
     * @param pcf_rule PCF rule in API format
     * @return Parsed PCC rule or nullptr if parsing failed
     */
    std::shared_ptr<PccRule> parse_rule_from_pcf_format(
        const nlohmann::json& pcf_rule);
    
    /**
     * @brief Validate a PCC rule
     * @param rule PCC rule to validate
     * @return true if rule is valid, false otherwise
     */
    bool validate_rule(const PccRule& rule);

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Logger
    std::shared_ptr<spdlog::logger> logger_;
    
    // Cached rules
    std::unordered_map<std::string, PccRule> rules_cache_;
    std::mutex rules_cache_mutex_;
    
    /**
     * @brief Generate a unique rule ID
     * @return A unique rule ID
     */
    std::string generate_rule_id();
};

} // namespace southbound
} // namespace af