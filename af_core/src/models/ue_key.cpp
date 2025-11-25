#include "../../include/models/ue_state.h"
#include <regex>
#include <stdexcept>

// ============================================================================
// UeKey Implementation
// ============================================================================

UeKey::UeKey(const std::string& key, KeyType key_type) : type_(key_type) {
    switch (key_type) {
        case KeyType::SUPI_BASED:
            validate_supi_format(key);
            supi_value_ = key;
            break;

        case KeyType::GPSI_BASED:
            validate_gpsi_format(key);
            gpsi_value_ = key;
            break;

        case KeyType::IPV4_BASED:
            validate_ipv4_format(key);
            ipv4_value_ = key;
            break;

        case KeyType::IPV6_BASED:
            validate_ipv6_format(key);
            ipv6_value_ = key;
            break;

        case KeyType::IPV6_PREFIX_BASED:
            validate_ipv6_prefix_format(key);
            ipv6_prefix_value_ = key;
            break;

        case KeyType::MAC_ADDR_BASED:
            validate_mac_format(key);
            mac_addr_value_ = key;
            break;

        case KeyType::COMPOSITE:
            throw std::invalid_argument("COMPOSITE key type requires explicit construction with multiple identifiers");

        default:
            throw std::invalid_argument("Unknown key type");
    }
}

std::string UeKey::get_primary_key() const {
    switch (type_) {
        case KeyType::SUPI_BASED:
            return "supi:" + supi_value_.value_or("");

        case KeyType::GPSI_BASED:
            return "gpsi:" + gpsi_value_.value_or("");

        case KeyType::IPV4_BASED:
            return "ipv4:" + ipv4_value_.value_or("");

        case KeyType::IPV6_BASED:
            return "ipv6:" + ipv6_value_.value_or("");

        case KeyType::IPV6_PREFIX_BASED:
            return "ipv6_prefix:" + ipv6_prefix_value_.value_or("");

        case KeyType::MAC_ADDR_BASED:
            return "mac:" + mac_addr_value_.value_or("");

        case KeyType::COMPOSITE: {
            // Hierarchical fallback for composite keys
            if (supi_value_) return "supi:" + *supi_value_;
            if (gpsi_value_) return "gpsi:" + *gpsi_value_;
            if (ipv4_value_) return "ipv4:" + *ipv4_value_;
            if (ipv6_value_) return "ipv6:" + *ipv6_value_;
            if (ipv6_prefix_value_) return "ipv6_prefix:" + *ipv6_prefix_value_;
            if (mac_addr_value_) return "mac:" + *mac_addr_value_;
            return "unknown";
        }
    }
    return "unknown";
}

// ============================================================================
// Validation Implementations
// ============================================================================

void UeKey::validate_supi_format(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument("SUPI value cannot be empty");
    }

    // SUPI format: either "imsi-<digits>" or "nai-<email-like>"
    // Basic validation - can be enhanced with more strict regex
    if (value.find("imsi-") == 0) {
        // IMSI should have 15 digits after "imsi-"
        std::string imsi_digits = value.substr(5); // Skip "imsi-"
        if (imsi_digits.empty() || imsi_digits.length() > 15) {
            throw std::invalid_argument("Invalid IMSI format in SUPI: must be imsi-<5-15 digits>");
        }
        // Check if all characters after "imsi-" are digits
        if (!std::all_of(imsi_digits.begin(), imsi_digits.end(), ::isdigit)) {
            throw std::invalid_argument("Invalid IMSI format in SUPI: digits only after imsi-");
        }
    } else if (value.find("nai-") == 0) {
        // NAI should have email-like format
        std::string nai_part = value.substr(4); // Skip "nai-"
        if (nai_part.empty() || nai_part.find('@') == std::string::npos) {
            throw std::invalid_argument("Invalid NAI format in SUPI: must be nai-<username>@<domain>");
        }
    } else {
        throw std::invalid_argument("Invalid SUPI format: must start with 'imsi-' or 'nai-'");
    }
}

void UeKey::validate_gpsi_format(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument("GPSI value cannot be empty");
    }

    // GPSI format: typically E.164 format with '+' or user@domain format
    if (value.find("+") == 0) {
        std::string e164 = value.substr(1); // Skip leading '+'
        if (e164.empty() || e164.length() > 15) {
            throw std::invalid_argument("Invalid E.164 format in GPSI: must be +<digits>");
        }
        if (!std::all_of(e164.begin(), e164.end(), ::isdigit)) {
            throw std::invalid_argument("Invalid E.164 format in GPSI: digits only after +");
        }
        return;
    }

    size_t at_pos = value.find('@');
    if (at_pos != std::string::npos) { // user@domain format
        if (at_pos == std::string::npos || at_pos == 0 || at_pos == value.length() - 1) {
            throw std::invalid_argument("Invalid user@domain format in GPSI: must be <user>@<domain>");
        }
        return;
    } else {
        throw std::invalid_argument("Invalid GPSI format: must start with 'e164-' or be in user@domain format");
    }
}

void UeKey::validate_ipv4_format(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument("IPv4 address cannot be empty");
    }

    // Simple IPv4 validation using regex
    // Pattern: 0-255.0-255.0-255.0-255
    static const std::regex ipv4_pattern(
        R"(^((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])$)"
    );

    if (!std::regex_match(value, ipv4_pattern)) {
        throw std::invalid_argument("Invalid IPv4 address format: " + value);
    }
}

void UeKey::validate_ipv6_format(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument("IPv6 address cannot be empty");
    }

    // Basic IPv6 validation - must contain colons
    // More sophisticated validation can be added with full IPv6 regex
    if (value.find(':') == std::string::npos) {
        throw std::invalid_argument("Invalid IPv6 address format: must contain colons");
    }

    // Check for valid hexadecimal characters and colons
    static const std::regex ipv6_pattern(
        R"(^([0-9a-fA-F]{0,4}:){2,7}([0-9a-fA-F]{0,4}|((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9]))$)"
    );

    if (!std::regex_match(value, ipv6_pattern)) {
        throw std::invalid_argument("Invalid IPv6 address format: " + value);
    }
}

void UeKey::validate_ipv6_prefix_format(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument("IPv6 prefix cannot be empty");
    }

    // IPv6 prefix must contain '/' for CIDR notation
    size_t slash_pos = value.find('/');
    if (slash_pos == std::string::npos) {
        throw std::invalid_argument("Invalid IPv6 prefix format: must contain '/' for CIDR notation");
    }

    // Validate the address part
    std::string address_part = value.substr(0, slash_pos);
    std::string prefix_length = value.substr(slash_pos + 1);

    if (address_part.empty() || prefix_length.empty()) {
        throw std::invalid_argument("Invalid IPv6 prefix format: empty address or prefix length");
    }

    // Validate prefix length is a number between 0 and 128
    try {
        int length = std::stoi(prefix_length);
        if (length < 0 || length > 128) {
            throw std::invalid_argument("IPv6 prefix length must be between 0 and 128");
        }
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid IPv6 prefix length: " + prefix_length);
    }

    // Validate the IPv6 address part
    validate_ipv6_format(address_part);
}

void UeKey::validate_mac_format(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument("MAC address cannot be empty");
    }

    // MAC address formats:
    // - XX:XX:XX:XX:XX:XX (colon-separated)
    // - XX-XX-XX-XX-XX-XX (dash-separated)
    static const std::regex mac_pattern_colon(
        R"(^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$)"
    );
    static const std::regex mac_pattern_dash(
        R"(^([0-9A-Fa-f]{2}-){5}[0-9A-Fa-f]{2}$)"
    );

    if (!std::regex_match(value, mac_pattern_colon) &&
        !std::regex_match(value, mac_pattern_dash)) {
        throw std::invalid_argument("Invalid MAC address format: " + value +
                                   " (expected XX:XX:XX:XX:XX:XX or XX-XX-XX-XX-XX-XX)");
    }
}
