#ifndef AF_CORE_MODEL_UE_STATE_H
#define AF_CORE_MODEL_UE_STATE_H


#include <string>
#include <vector>
#include <optional>
#include <functional> // Required for std::hash
#include <unordered_set>
#include <variant>
#include <chrono>
#include <stdexcept>
#include "../../common/models/common.h"

/**
 * @brief Universal UE Key for state management (Value Object)
 * @details Provides a flexible, immutable key system that can identify UEs by different
 * identifiers when SUPI is not available. Uses a hierarchical approach where SUPI is
 * preferred but IP addresses, GPSI, or other identifiers can serve as temporary keys.
 *
 * @note This is a Value Object - equality is based on the primary key value
 * @thread_safety Read operations are thread-safe. Promotion should be externally synchronized.
 */
class UeKey {
public:
    /**
     * @brief Enumeration of supported key types in hierarchical order
     */
    enum class KeyType {
        SUPI_BASED,         ///< Primary: UE identified by SUPI (Subscription Permanent Identifier)
        GPSI_BASED,         ///< Secondary: UE identified by GPSI (Generic Public Subscription Identifier)
        IPV4_BASED,         ///< Tertiary: UE identified by IPv4 address
        IPV6_BASED,         ///< Tertiary: UE identified by IPv6 address
        IPV6_PREFIX_BASED,  ///< Tertiary: UE identified by IPv6 prefix
        MAC_ADDR_BASED,     ///< Tertiary: UE identified by MAC address (Ethernet PDU sessions)
        COMPOSITE           ///< Multiple identifiers available (for future use)
    };

    // ============================================================================
    // Factory Methods
    // ============================================================================

    /**
     * @brief Create a SUPI-based key (preferred identifier)
     * @param supi Subscription Permanent Identifier
     * @return UeKey instance
     * @throws std::invalid_argument if SUPI format is invalid
     */
    static UeKey from_supi(const Supi& supi) {
        validate_supi_format(supi.value);
        return UeKey(KeyType::SUPI_BASED, supi.value, {}, {}, {}, {}, {});
    }

    /**
     * @brief Create a GPSI-based key (phone number)
     * @param gpsi Generic Public Subscription Identifier
     * @return UeKey instance
     * @throws std::invalid_argument if GPSI format is invalid
     */
    static UeKey from_gpsi(const Gpsi& gpsi) {
        validate_gpsi_format(gpsi.value);
        return UeKey(KeyType::GPSI_BASED, {}, gpsi.value, {}, {}, {}, {});
    }

    /**
     * @brief Create an IPv4-based key
     * @param ipv4 IPv4 address
     * @return UeKey instance
     * @throws std::invalid_argument if IPv4 format is invalid
     */
    static UeKey from_ipv4(const Ipv4Addr& ipv4) {
        validate_ipv4_format(ipv4.value);
        return UeKey(KeyType::IPV4_BASED, {}, {}, ipv4.value, {}, {}, {});
    }

    /**
     * @brief Create an IPv6-based key
     * @param ipv6 IPv6 address
     * @return UeKey instance
     * @throws std::invalid_argument if IPv6 format is invalid
     */
    static UeKey from_ipv6(const Ipv6Addr& ipv6) {
        validate_ipv6_format(ipv6.value);
        return UeKey(KeyType::IPV6_BASED, {}, {}, {}, ipv6.value, {}, {});
    }

    /**
     * @brief Create an IPv6 prefix-based key
     * @param ipv6_prefix IPv6 prefix with CIDR notation
     * @return UeKey instance
     * @throws std::invalid_argument if IPv6 prefix format is invalid
     */
    static UeKey from_ipv6_prefix(const Ipv6Prefix& ipv6_prefix) {
        validate_ipv6_prefix_format(ipv6_prefix.value);
        return UeKey(KeyType::IPV6_PREFIX_BASED, {}, {}, {}, {}, ipv6_prefix.value, {});
    }

    /**
     * @brief Create a MAC address-based key
     * @param mac_addr MAC address (for Ethernet PDU sessions)
     * @return UeKey instance
     * @throws std::invalid_argument if MAC address format is invalid
     */
    static UeKey from_mac(const MacAddr48& mac_addr) {
        validate_mac_format(mac_addr.value);
        return UeKey(KeyType::MAC_ADDR_BASED, {}, {}, {}, {}, {}, mac_addr.value);
    }

    // ============================================================================
    // Legacy Constructors
    // ============================================================================

    /**
     * @brief Constructor for SUPI-based key (legacy)
     * @param supi Subscription Permanent Identifier
     * @deprecated Use UeKey::from_supi() instead
     */
    explicit UeKey(const Supi& supi)
        : type_(KeyType::SUPI_BASED), supi_value_(supi.value) {
        validate_supi_format(supi.value);
    }

    /**
     * @brief Constructor for GPSI-based key (legacy)
     * @param gpsi Generic Public Subscription Identifier
     * @deprecated Use UeKey::from_gpsi() instead
     */
    explicit UeKey(const Gpsi& gpsi)
        : type_(KeyType::GPSI_BASED), gpsi_value_(gpsi.value) {
        validate_gpsi_format(gpsi.value);
    }

    /**
     * @brief Constructor for IPv4-based key (legacy)
     * @param ipv4 IPv4 address
     * @deprecated Use UeKey::from_ipv4() instead
     */
    explicit UeKey(const Ipv4Addr& ipv4)
        : type_(KeyType::IPV4_BASED), ipv4_value_(ipv4.value) {
        validate_ipv4_format(ipv4.value);
    }

    /**
     * @brief Constructor for IPv6-based key (legacy)
     * @param ipv6 IPv6 address
     * @deprecated Use UeKey::from_ipv6() instead
     */
    explicit UeKey(const Ipv6Addr& ipv6)
        : type_(KeyType::IPV6_BASED), ipv6_value_(ipv6.value) {
        validate_ipv6_format(ipv6.value);
    }

    /**
     * @brief Constructor for IPv6 Prefix-based key (legacy)
     * @param ipv6_prefix IPv6 prefix with CIDR notation
     * @deprecated Use UeKey::from_ipv6_prefix() instead
     */
    explicit UeKey(const Ipv6Prefix& ipv6_prefix)
        : type_(KeyType::IPV6_PREFIX_BASED), ipv6_prefix_value_(ipv6_prefix.value) {
        validate_ipv6_prefix_format(ipv6_prefix.value);
    }

    /**
     * @brief Constructor for MAC Address-based key (legacy)
     * @param mac_addr MAC address
     * @deprecated Use UeKey::from_mac() instead
     */
    explicit UeKey(const MacAddr48& mac_addr)
        : type_(KeyType::MAC_ADDR_BASED), mac_addr_value_(mac_addr.value) {
        validate_mac_format(mac_addr.value);
    }

    /**
     * @brief Generic constructor with string value and type
     * @param key The key value as string
     * @param key_type The type of key
     * @throws std::invalid_argument if key format doesn't match the type
     * @note Consider using factory methods for better type safety
     */
    UeKey(const std::string& key, KeyType key_type);

    // ============================================================================
    // Accessors (Read-only Access to State)
    // ============================================================================

    /**
     * @brief Get the type of this key
     * @return KeyType enumeration value
     */
    KeyType get_type() const noexcept { return type_; }

    /**
     * @brief Get the primary key string for map indexing
     * @return Formatted key string (e.g., "supi:imsi-123456789")
     */
    std::string get_primary_key() const;

    /**
     * @brief Get SUPI value if present
     * @return Optional SUPI string
     */
    const std::optional<std::string>& get_supi_value() const noexcept { return supi_value_; }

    /**
     * @brief Get GPSI value if present
     * @return Optional GPSI string
     */
    const std::optional<std::string>& get_gpsi_value() const noexcept { return gpsi_value_; }

    /**
     * @brief Get IPv4 value if present
     * @return Optional IPv4 address string
     */
    const std::optional<std::string>& get_ipv4_value() const noexcept { return ipv4_value_; }

    /**
     * @brief Get IPv6 value if present
     * @return Optional IPv6 address string
     */
    const std::optional<std::string>& get_ipv6_value() const noexcept { return ipv6_value_; }

    /**
     * @brief Get IPv6 prefix value if present
     * @return Optional IPv6 prefix string
     */
    const std::optional<std::string>& get_ipv6_prefix_value() const noexcept { return ipv6_prefix_value_; }

    /**
     * @brief Get MAC address value if present
     * @return Optional MAC address string
     */
    const std::optional<std::string>& get_mac_addr_value() const noexcept { return mac_addr_value_; }

    // ============================================================================
    // State Queries
    // ============================================================================

    /**
     * @brief Check if this key can be promoted to SUPI-based
     * @return true if key is not SUPI-based but has SUPI value available
     */
    bool can_promote_to_supi() const noexcept {
        return type_ != KeyType::SUPI_BASED && supi_value_.has_value();
    }

    /**
     * @brief Check if this key is provisional (not SUPI-based)
     * @return true if key is not SUPI-based
     */
    bool is_provisional() const noexcept {
        return type_ != KeyType::SUPI_BASED;
    }

    /**
     * @brief Check if this key is fully resolved (SUPI-based)
     * @return true if key is SUPI-based
     */
    bool is_resolved() const noexcept {
        return type_ == KeyType::SUPI_BASED;
    }

    // ============================================================================
    // State Mutations (Limited to SUPI Promotion and Type Demotion)
    // ============================================================================

    /**
     * @brief Promote this key to SUPI-based if possible
     * @note This is the ONLY mutable operation allowed
     * @thread_safety This operation should be externally synchronized
     */
    void promote_to_supi() {
        if (can_promote_to_supi()) {
            type_ = KeyType::SUPI_BASED;
        }
    }

    /**
     * @brief Demote this key to COMPOSITE type
     * @note Used when the primary identifier is removed but other identifiers remain
     * @thread_safety This operation should be externally synchronized
     */
    void demote_to_composite() {
        if (type_ != KeyType::COMPOSITE) {
            type_ = KeyType::COMPOSITE;
        }
    }

    /**
     * @brief Create a new UeKey promoted to SUPI (immutable pattern)
     * @return New UeKey instance with SUPI promotion, or copy if already SUPI-based
     * @note Preferred over mutable promote_to_supi() for functional style
     */
    UeKey with_supi_promotion() const {
        if (can_promote_to_supi()) {
            return UeKey(KeyType::SUPI_BASED, supi_value_, gpsi_value_,
                        ipv4_value_, ipv6_value_, ipv6_prefix_value_, mac_addr_value_);
        }
        return *this;
    }

    /**
     * @brief Create a new composite UeKey (immutable pattern)
     * @return New UeKey instance with COMPOSITE type
     * @note Preferred over mutable demote_to_composite() for functional style
     */
    UeKey as_composite() const {
        if (type_ == KeyType::COMPOSITE) {
            return *this;
        }
        return UeKey(KeyType::COMPOSITE, supi_value_, gpsi_value_,
                    ipv4_value_, ipv6_value_, ipv6_prefix_value_, mac_addr_value_);
    }

    // ============================================================================
    // Comparison Operators (Value Object Semantics)
    // ============================================================================

    /**
     * @brief Equality comparison based on primary key
     * @param other UeKey to compare with
     * @return true if primary keys are equal
     */
    bool operator==(const UeKey& other) const {
        return get_primary_key() == other.get_primary_key();
    }

    /**
     * @brief Inequality comparison
     * @param other UeKey to compare with
     * @return true if primary keys are not equal
     */
    bool operator!=(const UeKey& other) const {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison for ordered containers
     * @param other UeKey to compare with
     * @return true if this key is lexicographically less than other
     */
    bool operator<(const UeKey& other) const {
        return get_primary_key() < other.get_primary_key();
    }

private:
    // ============================================================================
    // Private Members
    // ============================================================================

    KeyType type_;                                  ///< Type of key (determines which value is primary)
    std::optional<std::string> supi_value_;        ///< SUPI value if available
    std::optional<std::string> gpsi_value_;        ///< GPSI value if available
    std::optional<std::string> ipv4_value_;        ///< IPv4 address if available
    std::optional<std::string> ipv6_value_;        ///< IPv6 address if available
    std::optional<std::string> ipv6_prefix_value_; ///< IPv6 prefix if available
    std::optional<std::string> mac_addr_value_;    ///< MAC address if available

    // ============================================================================
    // Private Constructor (Used by Factory Methods)
    // ============================================================================

    /**
     * @brief Private constructor for controlled object creation
     * @note Used by factory methods to ensure validation
     */
    UeKey(KeyType type,
          std::optional<std::string> supi,
          std::optional<std::string> gpsi,
          std::optional<std::string> ipv4,
          std::optional<std::string> ipv6,
          std::optional<std::string> ipv6_prefix,
          std::optional<std::string> mac_addr)
        : type_(type), supi_value_(std::move(supi)), gpsi_value_(std::move(gpsi)),
          ipv4_value_(std::move(ipv4)), ipv6_value_(std::move(ipv6)),
          ipv6_prefix_value_(std::move(ipv6_prefix)), mac_addr_value_(std::move(mac_addr)) {}

    // ============================================================================
    // Validation Helpers (Static Methods)
    // ============================================================================

    /**
     * @brief Validate SUPI format
     * @param value SUPI string to validate
     * @throws std::invalid_argument if format is invalid
     */
    static void validate_supi_format(const std::string& value);

    /**
     * @brief Validate GPSI format
     * @param value GPSI string to validate
     * @throws std::invalid_argument if format is invalid
     */
    static void validate_gpsi_format(const std::string& value);

    /**
     * @brief Validate IPv4 address format
     * @param value IPv4 string to validate
     * @throws std::invalid_argument if format is invalid
     */
    static void validate_ipv4_format(const std::string& value);

    /**
     * @brief Validate IPv6 address format
     * @param value IPv6 string to validate
     * @throws std::invalid_argument if format is invalid
     */
    static void validate_ipv6_format(const std::string& value);

    /**
     * @brief Validate IPv6 prefix format
     * @param value IPv6 prefix string to validate
     * @throws std::invalid_argument if format is invalid
     */
    static void validate_ipv6_prefix_format(const std::string& value);

    /**
     * @brief Validate MAC address format
     * @param value MAC address string to validate
     * @throws std::invalid_argument if format is invalid
     */
    static void validate_mac_format(const std::string& value);
};

// --- Hash specializations for custom structs to use with std::unordered_map ---
/**
 * @brief Represents information about a PDU Session, including associated IP addresses.
 * @details This structure combines data often reported together for a PDU Session from:
 * PduSessionManagementData (3GPP TS 29.519), PduSessionEventNotification (3GPP TS 29.514),
 * and Nupf_GetUEPrivateIPaddrAndIdentifiers (3GPP TS 29.564).
 */
struct PduSessionData {
    std::string pdu_session_id; // PDU Session Identifier
    Dnn dnn; // Data Network Name for the PDU session
    std::optional<Snssai> snssai; // S-NSSAI for the PDU session
    std::string pdu_session_type; // PDU Session type (e.g., "IPV4", "IPV6", "ETHERNET")
    std::string status; // State of the PDU session ("ACTIVE", "RELEASED")
    std::optional<DateTime> status_timestamp; // Timestamp of PDU session status update

    // UE IP Address / MAC Address related to this PDU session
    std::optional<Ipv4Addr> ue_ipv4_address; // UE IPv4 address for this session
    std::vector<Ipv6Prefix> ue_ipv6_prefixes; // UE IPv6 prefixes for this session
    std::vector<Ipv6Addr> ue_ipv6_addresses; // Individual UE IPv6 addresses for this session
    std::optional<MacAddr48> ue_mac_address; // UE MAC address for this session (for Ethernet PDU sessions)
    std::optional<DateTime> ip_address_timestamp; // Timestamp when IP address was last updated for this session
    std::optional<int> public_port_number; // UDP or TCP port associated with the public address (from Nupf_GetUEPrivateIPaddrAndIdentifiers)

    std::optional<std::string> dnai; // Data Network Access Identifier (DNAI) for the PDU session
    std::optional<DateTime> dnai_timestamp; // Timestamp of DNAI update
    std::vector<RouteToLocation> n6_traffic_routing_info; // N6 traffic routing requirement for the PDU session
    std::optional<DateTime> n6_traffic_routing_info_timestamp; // Timestamp for N6 traffic routing info update
    std::unordered_set<std::string> active_qod_session_ids; // Set of active QoD session IDs associated with this PDU session

    bool operator==(const PduSessionData& other) const {
        return pdu_session_id == other.pdu_session_id &&
               dnn == other.dnn &&
               snssai == other.snssai &&
               pdu_session_type == other.pdu_session_type &&
               status == other.status &&
               status_timestamp == other.status_timestamp &&
               ue_ipv4_address == other.ue_ipv4_address &&
               ue_ipv6_prefixes == other.ue_ipv6_prefixes &&
               ue_ipv6_addresses == other.ue_ipv6_addresses &&
               ue_mac_address == other.ue_mac_address &&
               ip_address_timestamp == other.ip_address_timestamp &&
               public_port_number == other.public_port_number &&
               dnai == other.dnai &&
               dnai_timestamp == other.dnai_timestamp &&
               n6_traffic_routing_info == other.n6_traffic_routing_info &&
               n6_traffic_routing_info_timestamp == other.n6_traffic_routing_info_timestamp;
    }
};

/**
 * @brief Represents Access and Mobility Subscription Data for a UE.
 * @details Derived from AccessAndMobilityData (3GPP TS 29.519) and related information from other specifications.
 * @source 3GPP TS 29.505, 3GPP TS 29.519
 */
struct UeAccessMobilityData {
    std::optional<AccessType> access_type; // Type of access (e.g., 3GPP_ACCESS, NON_3GPP_ACCESS)
    std::vector<AccessType> access_types_list; // List of access types if multiple are relevant
    std::optional<RatType> rat_type; // Radio Access Technology type
    std::vector<RatType> rat_types_list; // List of RAT types if multiple are relevant
    std::optional<PlmnIdNid> serving_plmn; // Serving PLMN ID (and NID for SNPN)
    std::optional<DateTime> current_plmn_timestamp; // Timestamp of current PLMN data
    std::optional<bool> roaming_status; // True if serving PLMN is different from HPLMN
    std::optional<DateTime> roaming_status_timestamp; // Timestamp of roaming status update
    std::optional<std::string> connectivity_management_state; // Connection Management State (CmInfo)
    std::optional<DateTime> cm_state_timestamp; // Timestamp of CM state data
    std::optional<std::string> registration_management_state; // Registration Management State (RmInfo)
    std::optional<std::string> ue_reachability_status; // UE reachability status
    std::optional<DateTime> reachability_status_timestamp; // Timestamp of reachability status data
    std::optional<std::string> sms_support; // Indicates supported SMS delivery of a UE
    std::optional<DateTime> sms_support_timestamp; // Timestamp of SMS support data
    std::optional<std::string> satellite_backhaul_category; // Indicates satellite backhaul category (e.g., LEO, MEO, GEO)

    bool operator==(const UeAccessMobilityData& other) const {
        return access_type == other.access_type &&
               access_types_list == other.access_types_list &&
               rat_type == other.rat_type &&
               rat_types_list == other.rat_types_list &&
               serving_plmn == other.serving_plmn &&
               current_plmn_timestamp == other.current_plmn_timestamp &&
               roaming_status == other.roaming_status &&
               roaming_status_timestamp == other.roaming_status_timestamp &&
               connectivity_management_state == other.connectivity_management_state &&
               cm_state_timestamp == other.cm_state_timestamp &&
               registration_management_state == other.registration_management_state &&
               ue_reachability_status == other.ue_reachability_status &&
               reachability_status_timestamp == other.reachability_status_timestamp &&
               sms_support == other.sms_support &&
               sms_support_timestamp == other.sms_support_timestamp &&
               satellite_backhaul_category == other.satellite_backhaul_category;
    }
};

/**
 * @brief A comprehensive structure for an Application Function (AF) to maintain
 * periodically updated UE subscription information.
 * @details This struct combines various UE-specific data points obtainable via
 * subscriptions, allowing the AF to have a holistic view of UE status,
 * IP addresses, and network context. It uses a flexible key system to handle
 * cases where SUPI is not initially available.
 */
struct AfUeSubscriptionState {
    // **UE Identity Management**
    UeKey ue_key; // Primary key for this UE state
    std::optional<Supi> supi; // Subscription Permanent Identifier (may be resolved later)
    std::optional<Gpsi> gpsi; // Generic Public Subscription Identifier (GPSI)

    // **Resolution State**
    enum class ResolutionState {
        PROVISIONAL,    // UE identified by IP/temporary identifier only
        PARTIAL,        // UE has some identifiers but not SUPI
        RESOLVED        // UE has SUPI and full identity
    };
    ResolutionState resolution_state;
    std::optional<std::chrono::system_clock::time_point> supi_resolved_at; // When SUPI was resolved

    // **Alternative Identifiers** (for cross-referencing and promotion)
    std::vector<std::string> known_ipv4_addresses; // All known IPv4 addresses for this UE
    std::vector<std::string> known_ipv6_addresses; // All known IPv6 addresses for this UE
    std::vector<std::string> known_gpsi_values;    // All known GPSI values for this UE
    std::vector<std::string> known_ipv6_prefixes;  // All known IPv6 prefixes for this UE
    std::vector<std::string> known_mac_addresses;   // All known MAC addresses for this UE

    // **UE Network Location and Access Information**
    std::optional<UeLocationInfo> location_info; // UE's last reported location and time zone
    std::optional<std::chrono::system_clock::time_point> location_timestamp; // Timestamp when the location was last updated
    std::optional<UeAccessMobilityData> access_mobility_data; // UE's access and mobility related data

    // **PDU Session(s) and Associated IP/MAC Addresses**
    // A UE can have multiple PDU Sessions, each with its own IP/MAC and network context.
    std::vector<PduSessionData> pdu_sessions; // List of active PDU sessions with their details

    // **UE Policy and Application Data Relevant to Network Interaction**
    std::optional<std::string> ue_policy_data_info; // Represents the UE Policy data, e.g., UE Policy Set, URSP rules, as a string or more specific struct if detailed parsing is needed
    std::optional<PduidInformation> prose_pduid_information; // ProSe Discovery UE ID and its validity timer for UE ProSe Policies

    // **Service-Related Capabilities and States**
    std::optional<bool> time_sync_service_available_and_capable; // Indicates if 5GS/UE supports time synchronization service
    std::vector<ServiceAreaCoverageInfo> service_area_coverage_allowed; // List of Tracking Areas per serving network where service is allowed
    std::optional<bool> high_throughput_desired_for_ue_traffic; // Indicates if high throughput is desired for indicated UE traffic

    // **State Management Metadata**
    std::chrono::system_clock::time_point created_at; // When this state entry was first created
    std::chrono::system_clock::time_point last_updated; // When this state was last modified
    std::optional<std::chrono::system_clock::time_point> last_activity; // Last time UE had any network activity

    // Constructor for IP-based provisional state
    explicit AfUeSubscriptionState(const UeKey& key)
        : ue_key(key), resolution_state(ResolutionState::PROVISIONAL),
          created_at(std::chrono::system_clock::now()), last_updated(std::chrono::system_clock::now()) {}

    // Constructor for SUPI-based resolved state
    explicit AfUeSubscriptionState(const Supi& supi_val)
        : ue_key(UeKey(supi_val)), supi(supi_val), resolution_state(ResolutionState::RESOLVED),
          supi_resolved_at(std::chrono::system_clock::now()), created_at(std::chrono::system_clock::now()), last_updated(std::chrono::system_clock::now()) {}

    // Method to promote provisional state when SUPI is resolved
    void resolve_supi(const Supi& supi_val) {
        if (!supi.has_value()) {
            supi = supi_val;
            resolution_state = ResolutionState::RESOLVED;
            supi_resolved_at = std::chrono::system_clock::now();

            // Update the key if it can be promoted
            // Note: Since UeKey is now properly encapsulated, we can only promote if the
            // SUPI value is already present in the key. If not, we need to create a new key.
            if (ue_key.can_promote_to_supi()) {
                ue_key.promote_to_supi();
            } else {
                // Create a new SUPI-based key if the current key doesn't have SUPI value
                ue_key = UeKey::from_supi(supi_val);
            }
        }
    }

    // Method to add alternative identifiers for cross-referencing
    void add_known_identifier(const std::string& type, const std::string& value) {
        if (type == "ipv4" && std::find(known_ipv4_addresses.begin(), known_ipv4_addresses.end(), value) == known_ipv4_addresses.end()) {
            known_ipv4_addresses.push_back(value);
        } else if (type == "ipv6" && std::find(known_ipv6_addresses.begin(), known_ipv6_addresses.end(), value) == known_ipv6_addresses.end()) {
            known_ipv6_addresses.push_back(value);
        } else if (type == "gpsi" && std::find(known_gpsi_values.begin(), known_gpsi_values.end(), value) == known_gpsi_values.end()) {
            known_gpsi_values.push_back(value);
        } else if (type == "ipv6_prefix" && std::find(known_ipv6_prefixes.begin(), known_ipv6_prefixes.end(), value) == known_ipv6_prefixes.end()) {
            known_ipv6_prefixes.push_back(value);
        } else if (type == "mac" && std::find(known_mac_addresses.begin(), known_mac_addresses.end(), value) == known_mac_addresses.end()) {
            known_mac_addresses.push_back(value);
        }
    }

    bool remove_known_identifier(const std::string& identifier_type, const std::string& identifier_value) {
        bool removed = false;
        bool should_demote = false;

        if (identifier_type == "ipv4") {
            auto it = std::find(known_ipv4_addresses.begin(), known_ipv4_addresses.end(), identifier_value);
            if (it != known_ipv4_addresses.end()) {
                known_ipv4_addresses.erase(it);
                removed = true;
                // Check if we need to demote: if the UeKey is IPv4-based and this was the last known IPv4
                if (ue_key.get_type() == UeKey::KeyType::IPV4_BASED && known_ipv4_addresses.empty()) {
                    should_demote = true;
                }
            }
        } else if (identifier_type == "ipv6") {
            auto it = std::find(known_ipv6_addresses.begin(), known_ipv6_addresses.end(), identifier_value);
            if (it != known_ipv6_addresses.end()) {
                known_ipv6_addresses.erase(it);
                removed = true;
                // Check if we need to demote: if the UeKey is IPv6-based and this was the last known IPv6
                if (ue_key.get_type() == UeKey::KeyType::IPV6_BASED && known_ipv6_addresses.empty()) {
                    should_demote = true;
                }
            }
        } else if (identifier_type == "gpsi") {
            auto it = std::find(known_gpsi_values.begin(), known_gpsi_values.end(), identifier_value);
            if (it != known_gpsi_values.end()) {
                known_gpsi_values.erase(it);
                removed = true;
                // Check if we need to demote: if the UeKey is GPSI-based and this was the last known GPSI
                if (ue_key.get_type() == UeKey::KeyType::GPSI_BASED && known_gpsi_values.empty()) {
                    should_demote = true;
                }
            }
        } else if (identifier_type == "ipv6_prefix") {
            auto it = std::find(known_ipv6_prefixes.begin(), known_ipv6_prefixes.end(), identifier_value);
            if (it != known_ipv6_prefixes.end()) {
                known_ipv6_prefixes.erase(it);
                removed = true;
                // Check if we need to demote: if the UeKey is IPv6-prefix-based and this was the last known IPv6-prefix
                if (ue_key.get_type() == UeKey::KeyType::IPV6_PREFIX_BASED && known_ipv6_prefixes.empty()) {
                    should_demote = true;
                }
            }
        } else if (identifier_type == "mac") {
            auto it = std::find(known_mac_addresses.begin(), known_mac_addresses.end(), identifier_value);
            if (it != known_mac_addresses.end()) {
                known_mac_addresses.erase(it);
                removed = true;
                // Check if we need to demote: if the UeKey is MAC-based and this was the last known MAC
                if (ue_key.get_type() == UeKey::KeyType::MAC_ADDR_BASED && known_mac_addresses.empty()) {
                    should_demote = true;
                }
            }
        }

        // Demote to composite if the primary identifier was removed but other identifiers exist
        if (should_demote) {
            ue_key.demote_to_composite();
        }

        return removed;
    }

    bool operator==(const AfUeSubscriptionState& other) const {
        return ue_key == other.ue_key &&
               supi == other.supi &&
               gpsi == other.gpsi &&
               location_info == other.location_info &&
               location_timestamp == other.location_timestamp &&
               access_mobility_data == other.access_mobility_data &&
               pdu_sessions == other.pdu_sessions &&
               ue_policy_data_info == other.ue_policy_data_info &&
               prose_pduid_information == other.prose_pduid_information &&
               time_sync_service_available_and_capable == other.time_sync_service_available_and_capable &&
               service_area_coverage_allowed == other.service_area_coverage_allowed &&
               high_throughput_desired_for_ue_traffic == other.high_throughput_desired_for_ue_traffic;
    }
};

// --- Hash specializations for custom structs to use with std::unordered_map ---
namespace std {

template<> struct hash<UeKey> {
    size_t operator()(const UeKey& key) const {
        return hash<string>{}(key.get_primary_key());
    }
};

template<> struct hash<Snssai> {
    size_t operator()(const Snssai& s) const {
        size_t h = hash<int>{}(s.sst);
        hash_combine(h, hash_optional(s.sd));
        return h;
    }
};

template<> struct hash<PlmnId> {
    size_t operator()(const PlmnId& p) const {
        size_t h = hash<string>{}(p.mcc);
        hash_combine(h, hash<string>{}(p.mnc));
        return h;
    }
};

template<> struct hash<PlmnIdNid> {
    size_t operator()(const PlmnIdNid& pn) const {
        size_t h = hash<PlmnId>{}(pn.plmn_id);
        hash_combine(h, hash_optional(pn.nid));
        return h;
    }
};

// Implement hash for UserLocation, RouteToLocation, PduidInformation, ServiceAreaCoverageInfo
template<> struct hash<UserLocation> {
    size_t operator()(const UserLocation& ul) const {
        size_t h = hash_optional(ul.location_area_5g);
        for (const auto& tac : ul.tai_list) {
            hash_combine(h, hash<Tac>{}(tac));
        }
        hash_combine(h, hash_optional(ul.geographical_area));
        return h;
    }
};

template<> struct hash<UeLocationInfo> {
    size_t operator()(const UeLocationInfo& uli) const {
        size_t h = hash<UserLocation>{}(uli.user_location);
        hash_combine(h, hash_optional(uli.time_zone));
        return h;
    }
};

template<> struct hash<RouteToLocation> {
    size_t operator()(const RouteToLocation& rtl) const {
        return hash<string>{}(rtl.dnai_identifier);
    }
};

template<> struct hash<PduidInformation> {
    size_t operator()(const PduidInformation& pi) const {
        size_t h = hash<DateTime>{}(pi.expiry);
        hash_combine(h, hash<string>{}(pi.pduid));
        return h;
    }
};

template<> struct hash<ServiceAreaCoverageInfo> {
    size_t operator()(const ServiceAreaCoverageInfo& saci) const {
        size_t h = 0;
        for (const auto& tac : saci.tac_list) {
            hash_combine(h, hash<Tac>{}(tac));
        }
        hash_combine(h, hash_optional(saci.serving_network));
        return h;
    }
};

// Hashing for vector of structs
template<typename T>
size_t hash_vector(const std::vector<T>& vec) {
    size_t h = 0;
    for (const auto& val : vec) {
        hash_combine(h, std::hash<T>{}(val));
    }
    return h;
}

template<> struct hash<PduSessionData> {
    size_t operator()(const PduSessionData& psd) const {
        size_t h = hash<string>{}(psd.pdu_session_id);
        hash_combine(h, hash<Dnn>{}(psd.dnn));
        hash_combine(h, hash_optional(psd.snssai));
        hash_combine(h, hash<string>{}(psd.pdu_session_type));
        hash_combine(h, hash<string>{}(psd.status));
        hash_combine(h, hash_optional(psd.status_timestamp));
        hash_combine(h, hash_optional(psd.ue_ipv4_address));
        hash_combine(h, hash_vector(psd.ue_ipv6_prefixes));
        hash_combine(h, hash_vector(psd.ue_ipv6_addresses));
        hash_combine(h, hash_optional(psd.ue_mac_address));
        hash_combine(h, hash_optional(psd.ip_address_timestamp));
        hash_combine(h, hash_optional(psd.public_port_number));
        hash_combine(h, hash_optional(psd.dnai));
        hash_combine(h, hash_optional(psd.dnai_timestamp));
        hash_combine(h, hash_vector(psd.n6_traffic_routing_info));
        hash_combine(h, hash_optional(psd.n6_traffic_routing_info_timestamp));
        return h;
    }
};

template<> struct hash<UeAccessMobilityData> {
    size_t operator()(const UeAccessMobilityData& uamd) const {
        size_t h = hash_optional(uamd.access_type);
        hash_combine(h, hash_vector(uamd.access_types_list));
        hash_combine(h, hash_optional(uamd.rat_type));
        hash_combine(h, hash_vector(uamd.rat_types_list));
        hash_combine(h, hash_optional(uamd.serving_plmn));
        hash_combine(h, hash_optional(uamd.current_plmn_timestamp));
        hash_combine(h, hash_optional(uamd.roaming_status));
        hash_combine(h, hash_optional(uamd.roaming_status_timestamp));
        hash_combine(h, hash_optional(uamd.connectivity_management_state));
        hash_combine(h, hash_optional(uamd.cm_state_timestamp));
        hash_combine(h, hash_optional(uamd.registration_management_state));
        hash_combine(h, hash_optional(uamd.ue_reachability_status));
        hash_combine(h, hash_optional(uamd.reachability_status_timestamp));
        hash_combine(h, hash_optional(uamd.sms_support));
        hash_combine(h, hash_optional(uamd.sms_support_timestamp));
        hash_combine(h, hash_optional(uamd.satellite_backhaul_category));
        return h;
    }
};

} // namespace std

#endif // AF_CORE_MODEL_UE_STATE_H