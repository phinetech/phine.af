/**
 * @file pcc_rule_manager.cpp
 * @brief Implementation of the PCC rule manager
 */

#include "pcc_rule_manager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <random>
#include <sstream>

namespace af {
namespace southbound {

PccRuleManager::PccRuleManager() {
    // Setup logger
    initializeLogger(spdlog::level::debug);

    logger_->info("PCC Rule Manager created");
}

PccRuleManager::~PccRuleManager() {
    // Cleanup
}

void PccRuleManager::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get("af_pcc_rule");

    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt("af_pcc_rule");
    }

    // Set the log level
    logger_->set_level(log_level);

    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

void PccRuleManager::initialize() {
    logger_->info("Initializing PCC Rule Manager");

    // Initialize any resources

    logger_->info("PCC Rule Manager initialized");
}

std::shared_ptr<PccRule> PccRuleManager::create_rule_from_media_component(
    const nlohmann::json& media_component) {

    logger_->debug("Creating PCC rule from media component");

    try {
        auto rule = std::make_shared<PccRule>();

        // Generate a unique rule ID
        rule->rule_id = generate_rule_id();

        // Set flow direction
        rule->flow_direction = "bidirectional";  // Default

        // Extract flow descriptions if available
        if (media_component.contains("fDescs") && media_component["fDescs"].is_array()) {
            for (const auto& flow : media_component["fDescs"]) {
                if (flow.contains("flowDescriptions") && flow["flowDescriptions"].is_array()) {
                    for (const auto& desc : flow["flowDescriptions"]) {
                        rule->flow_descriptions.push_back(desc);
                    }
                }
            }
        }

        // Extract QoS information if available
        if (media_component.contains("medQoS")) {
            if (media_component["medQoS"].contains("maxbrUl")) {
                rule->maximum_bandwidth_ul = media_component["medQoS"]["maxbrUl"];
            }

            if (media_component["medQoS"].contains("maxbrDl")) {
                rule->maximum_bandwidth_dl = media_component["medQoS"]["maxbrDl"];
            }

            if (media_component["medQoS"].contains("minbrUl")) {
                rule->guaranteed_bandwidth_ul = media_component["medQoS"]["minbrUl"];
            }

            if (media_component["medQoS"].contains("minbrDl")) {
                rule->guaranteed_bandwidth_dl = media_component["medQoS"]["minbrDl"];
            }
        }

        // Set default 5QI if not specified
        rule->qos_reference = 9;  // Default for non-GBR QoS

        // Validate the rule
        if (!validate_rule(*rule)) {
            logger_->error("Invalid PCC rule created from media component");
            return nullptr;
        }

        // Cache the rule
        {
            std::lock_guard<std::mutex> lock(rules_cache_mutex_);
            rules_cache_[rule->rule_id] = *rule;
        }

        logger_->debug("Created PCC rule with ID: {}", rule->rule_id);
        return rule;
    }
    catch (const std::exception& e) {
        logger_->error("Error creating PCC rule from media component: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<PccRule> PccRuleManager::create_rule_from_app_session(
    const nlohmann::json& app_session_data) {

    logger_->debug("Creating PCC rule from app session data");

    try {
        auto rule = std::make_shared<PccRule>();

        // Generate a unique rule ID
        rule->rule_id = generate_rule_id();

        // Set flow direction
        rule->flow_direction = "bidirectional";  // Default
        if (app_session_data.contains("flow_direction")) {
            rule->flow_direction = app_session_data["flow_direction"];
        }

        // Extract flow descriptions if available
        if (app_session_data.contains("flow_descriptions") &&
            app_session_data["flow_descriptions"].is_array()) {

            for (const auto& desc : app_session_data["flow_descriptions"]) {
                rule->flow_descriptions.push_back(desc);
            }
        }

        // Extract QoS information
        if (app_session_data.contains("qos_reference")) {
            rule->qos_reference = app_session_data["qos_reference"];
        } else {
            rule->qos_reference = 9;  // Default for non-GBR QoS
        }

        if (app_session_data.contains("guaranteed_bandwidth_ul")) {
            rule->guaranteed_bandwidth_ul = app_session_data["guaranteed_bandwidth_ul"];
        }

        if (app_session_data.contains("guaranteed_bandwidth_dl")) {
            rule->guaranteed_bandwidth_dl = app_session_data["guaranteed_bandwidth_dl"];
        }

        if (app_session_data.contains("maximum_bandwidth_ul")) {
            rule->maximum_bandwidth_ul = app_session_data["maximum_bandwidth_ul"];
        }

        if (app_session_data.contains("maximum_bandwidth_dl")) {
            rule->maximum_bandwidth_dl = app_session_data["maximum_bandwidth_dl"];
        }

        // Validate the rule
        if (!validate_rule(*rule)) {
            logger_->error("Invalid PCC rule created from app session data");
            return nullptr;
        }

        // Cache the rule
        {
            std::lock_guard<std::mutex> lock(rules_cache_mutex_);
            rules_cache_[rule->rule_id] = *rule;
        }

        logger_->debug("Created PCC rule with ID: {}", rule->rule_id);
        return rule;
    }
    catch (const std::exception& e) {
        logger_->error("Error creating PCC rule from app session data: {}", e.what());
        return nullptr;
    }
}

nlohmann::json PccRuleManager::convert_rule_to_pcf_format(const PccRule& rule) {
    logger_->debug("Converting PCC rule to PCF format: {}", rule.rule_id);

    nlohmann::json pcf_rule;

    // Set rule ID
    pcf_rule["pccRuleId"] = rule.rule_id;

    // Set flow information
    if (!rule.flow_descriptions.empty()) {
        nlohmann::json flow_infos = nlohmann::json::array();

        for (const auto& desc : rule.flow_descriptions) {
            nlohmann::json flow_info;
            flow_info["flowDescription"] = desc;
            flow_infos.push_back(flow_info);
        }

        pcf_rule["flowInfos"] = flow_infos;
    }

    // Set QoS information
    nlohmann::json qos_data;
    qos_data["5qi"] = rule.qos_reference;

    if (rule.guaranteed_bandwidth_ul > 0 || rule.guaranteed_bandwidth_dl > 0) {
        nlohmann::json gbr;

        if (rule.guaranteed_bandwidth_ul > 0) {
            gbr["gbrUl"] = std::to_string(rule.guaranteed_bandwidth_ul) + " Kbps";
        }

        if (rule.guaranteed_bandwidth_dl > 0) {
            gbr["gbrDl"] = std::to_string(rule.guaranteed_bandwidth_dl) + " Kbps";
        }

        qos_data["gbrUl"] = std::to_string(rule.guaranteed_bandwidth_ul) + " Kbps";
        qos_data["gbrDl"] = std::to_string(rule.guaranteed_bandwidth_dl) + " Kbps";
    }

    if (rule.maximum_bandwidth_ul > 0 || rule.maximum_bandwidth_dl > 0) {
        nlohmann::json mbr;

        if (rule.maximum_bandwidth_ul > 0) {
            mbr["mbrUl"] = std::to_string(rule.maximum_bandwidth_ul) + " Kbps";
        }

        if (rule.maximum_bandwidth_dl > 0) {
            mbr["mbrDl"] = std::to_string(rule.maximum_bandwidth_dl) + " Kbps";
        }

        qos_data["mbrUl"] = std::to_string(rule.maximum_bandwidth_ul) + " Kbps";
        qos_data["mbrDl"] = std::to_string(rule.maximum_bandwidth_dl) + " Kbps";
    }

    pcf_rule["qosData"] = qos_data;

    // Add additional parameters
    for (const auto& [key, value] : rule.additional_params) {
        pcf_rule[key] = value;
    }

    return pcf_rule;
}

std::shared_ptr<PccRule> PccRuleManager::parse_rule_from_pcf_format(
    const nlohmann::json& pcf_rule) {

    logger_->debug("Parsing PCC rule from PCF format");

    try {
        auto rule = std::make_shared<PccRule>();

        // Extract rule ID
        if (pcf_rule.contains("pccRuleId")) {
            rule->rule_id = pcf_rule["pccRuleId"];
        } else {
            rule->rule_id = generate_rule_id();
        }

        // Extract flow information
        if (pcf_rule.contains("flowInfos") && pcf_rule["flowInfos"].is_array()) {
            for (const auto& flow_info : pcf_rule["flowInfos"]) {
                if (flow_info.contains("flowDescription")) {
                    rule->flow_descriptions.push_back(flow_info["flowDescription"]);
                }
            }
        }

        // Extract QoS information
        if (pcf_rule.contains("qosData")) {
            const auto& qos_data = pcf_rule["qosData"];

            if (qos_data.contains("5qi")) {
                rule->qos_reference = qos_data["5qi"];
            }

            // Parse bandwidth values
            auto parse_bandwidth = [](const std::string& bw_str) -> int {
                try {
                    // TS 29.571: BitRate uses capital-K "Kbps"
                    size_t pos = bw_str.find(" Kbps");
                    if (pos == std::string::npos) {
                        // Fallback: also accept lowercase for backward compatibility
                        pos = bw_str.find(" kbps");
                    }
                    if (pos != std::string::npos) {
                        return std::stoi(bw_str.substr(0, pos));
                    }
                    return std::stoi(bw_str);
                } catch (...) {
                    return 0;
                }
            };

            if (qos_data.contains("gbrUl")) {
                rule->guaranteed_bandwidth_ul = parse_bandwidth(qos_data["gbrUl"]);
            }

            if (qos_data.contains("gbrDl")) {
                rule->guaranteed_bandwidth_dl = parse_bandwidth(qos_data["gbrDl"]);
            }

            if (qos_data.contains("mbrUl")) {
                rule->maximum_bandwidth_ul = parse_bandwidth(qos_data["mbrUl"]);
            }

            if (qos_data.contains("mbrDl")) {
                rule->maximum_bandwidth_dl = parse_bandwidth(qos_data["mbrDl"]);
            }
        }

        // Set default flow direction if not present
        rule->flow_direction = "bidirectional";

        // Cache the rule
        {
            std::lock_guard<std::mutex> lock(rules_cache_mutex_);
            rules_cache_[rule->rule_id] = *rule;
        }

        logger_->debug("Parsed PCC rule with ID: {}", rule->rule_id);
        return rule;
    }
    catch (const std::exception& e) {
        logger_->error("Error parsing PCC rule from PCF format: {}", e.what());
        return nullptr;
    }
}

bool PccRuleManager::validate_rule(const PccRule& rule) {
    // Basic validation
    if (rule.rule_id.empty()) {
        logger_->error("PCC rule has empty ID");
        return false;
    }

    // Validate flow direction
    if (rule.flow_direction != "uplink" &&
        rule.flow_direction != "downlink" &&
        rule.flow_direction != "bidirectional") {
        logger_->error("Invalid flow direction: {}", rule.flow_direction);
        return false;
    }

    // Validate QoS reference (5QI value)
    if (rule.qos_reference < 1 || rule.qos_reference > 79) {
        logger_->error("Invalid QoS reference (5QI): {}", rule.qos_reference);
        return false;
    }

    // Validate bandwidth values
    if (rule.guaranteed_bandwidth_ul < 0) {
        logger_->error("Guaranteed bandwidth UL cannot be negative: {}", rule.guaranteed_bandwidth_ul);
        return false;
    }

    if (rule.guaranteed_bandwidth_dl < 0) {
        logger_->error("Guaranteed bandwidth DL cannot be negative: {}", rule.guaranteed_bandwidth_dl);
        return false;
    }

    if (rule.maximum_bandwidth_ul < 0) {
        logger_->error("Maximum bandwidth UL cannot be negative: {}", rule.maximum_bandwidth_ul);
        return false;
    }

    if (rule.maximum_bandwidth_dl < 0) {
        logger_->error("Maximum bandwidth DL cannot be negative: {}", rule.maximum_bandwidth_dl);
        return false;
    }

    if (rule.maximum_bandwidth_ul > 0 && rule.guaranteed_bandwidth_ul > rule.maximum_bandwidth_ul) {
        logger_->error("Guaranteed bandwidth UL ({}) cannot exceed maximum bandwidth UL ({})",
                      rule.guaranteed_bandwidth_ul, rule.maximum_bandwidth_ul);
        return false;
    }

    if (rule.maximum_bandwidth_dl > 0 && rule.guaranteed_bandwidth_dl > rule.maximum_bandwidth_dl) {
        logger_->error("Guaranteed bandwidth DL ({}) cannot exceed maximum bandwidth DL ({})",
                      rule.guaranteed_bandwidth_dl, rule.maximum_bandwidth_dl);
        return false;
    }

    return true;
}

std::string PccRuleManager::generate_rule_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex_chars = "0123456789abcdef";

    std::stringstream ss;
    ss << "pcc-rule-";
    for (int i = 0; i < 8; ++i) {
        ss << hex_chars[dis(gen)];
    }

    return ss.str();
}

} // namespace southbound
} // namespace af