#ifndef AF_CORE_MODEL_COMMON_H
#define AF_CORE_MODEL_COMMON_H

#include <string>
#include <vector>
#include <optional>
#include <functional> // Required for std::hash

// --- Fundamental 3GPP Data Types (building blocks) ---

/**
 * @brief Represents a date-time string, formatted as defined in OpenAPI Specification.
 * @source 3GPP TS 29.505, 3GPP TS 29.519, 3GPP TS 29.591, 3GPP TS 29.572
 */
struct DateTime {
    std::string value;
    bool operator==(const DateTime& other) const { return value == other.value; }
};

/**
 * @brief Represents a Uniform Resource Identifier (URI).
 * @source 3GPP TS 29.514, 3GPP TS 29.517, 3GPP TS 29.519, 3GPP TS 29.525, 3GPP TS 29.534, 3GPP TS 29.557, 3GPP TS 29.572, 3GPP TS 29.591
 */
struct Uri {
    std::string value;
    bool operator==(const Uri& other) const { return value == other.value; }
};

/**
 * @brief Identifies a Single Network Slice Selection Assistance Information (S-NSSAI).
 * @source 3GPP TS 29.512, 3GPP TS 29.514, 3GPP TS 29.519, 3GPP TS 29.564, 3GPP TS 29.591
 */
struct Snssai {
    int sst; // Slice/Service Type
    std::optional<std::string> sd; // Slice Differentiator
    bool operator==(const Snssai& other) const {
        return sst == other.sst && sd == other.sd;
    }
};

/**
 * @brief Identifies a Data Network Name (DNN).
 * @source 3GPP TS 23.501, 3GPP TS 29.512, 3GPP TS 29.514, 3GPP TS 29.519, 3GPP TS 29.523, 3GPP TS 29.564, 3GPP TS 29.591
 */
struct Dnn {
    std::string value;
    bool operator==(const Dnn& other) const { return value == other.value; }
};

/**
 * @brief Represents an IPv4 Address.
 * @source 3GPP TS 29.512, 3GPP TS 29.519, 3GPP TS 29.525, 3GPP TS 29.564, 3GPP TS 29.591
 */
struct Ipv4Addr {
    std::string value;
    bool operator==(const Ipv4Addr& other) const { return value == other.value; }
};

/**
 * @brief Represents an IPv6 Address.
 * @source 3GPP TS 29.512, 3GPP TS 29.519, 3GPP TS 29.525, 3GPP TS 29.564, 3GPP TS 29.591
 */
struct Ipv6Addr {
    std::string value;
    bool operator==(const Ipv6Addr& other) const { return value == other.value; }
};

/**
 * @brief Represents an IPv6 Address Prefix.
 * @source 3GPP TS 29.512, 3GPP TS 29.519, 3GPP TS 29.564
 */
struct Ipv6Prefix {
    std::string value;
    bool operator==(const Ipv6Prefix& other) const { return value == other.value; }
};

/**
 * @brief Represents a MAC Address (48-bit).
 * @source 3GPP TS 29.514, 3GPP TS 29.523, 3GPP TS 29.564, 3GPP TS 29.519
 */
struct MacAddr48 {
    std::string value;
    bool operator==(const MacAddr48& other) const { return value == other.value; }
};

/**
 * @brief Represents a PLMN Identity.
 * @source 3GPP TS 29.519, 3GPP TS 29.591
 */
struct PlmnId {
    std::string mcc; // Mobile Country Code
    std::string mnc; // Mobile Network Code
    bool operator==(const PlmnId& other) const {
        return mcc == other.mcc && mnc == other.mnc;
    }
};

/**
 * @brief Identifies the network: the PLMN Identifier or for a SNPN, the NID.
 * @source 3GPP TS 29.512, 3GPP TS 29.523, 3GPP TS 29.525, 3GPP TS 29.534
 */
struct PlmnIdNid {
    PlmnId plmn_id; //
    std::optional<std::string> nid; // Network Identifier (for SNPN)
    bool operator==(const PlmnIdNid& other) const {
        return plmn_id == other.plmn_id && nid == other.nid;
    }
};

/**
 * @brief Represents a Tracking Area Code (TAC).
 * @source 3GPP TS 29.534, 3GPP TS 29.591
 */
struct Tac {
    std::string value;
    bool operator==(const Tac& other) const { return value == other.value; }
};

/**
 * @brief Identifies the access type, e.g., "3GPP_ACCESS" or "NON_3GPP_ACCESS".
 * @source 3GPP TS 29.512, 3GPP TS 29.519, 3GPP TS 29.525, 3GPP TS 29.572
 */
struct AccessType {
    std::string value;
    bool operator==(const AccessType& other) const { return value == other.value; }
};

/**
 * @brief Identifies a Radio Access Technology (RAT) type.
 * @source 3GPP TS 29.512, 3GPP TS 29.519, 3GPP TS 29.525, 3GPP TS 29.572
 */
struct RatType {
    std::string value;
    bool operator==(const RatType& other) const { return value == other.value; }
};

/**
 * @brief Represents an unsigned integer.
 * @source 3GPP TS 29.517, 3GPP TS 29.519, 3GPP TS 29.525, 3GPP TS 29.534, 3GPP TS 29.591
 */
struct Uinteger {
    unsigned int value;
    bool operator==(const Uinteger& other) const { return value == other.value; }
};

/**
 * @brief Represents the Subscription Permanent Identifier (SUPI) for a UE.
 * @source 3GPP TS 23.501, 3GPP TS 29.505, 3GPP TS 29.514, 3GPP TS 29.517, 3GPP TS 29.519, 3GPP TS 29.525, 3GPP TS 29.534, 3GPP TS 29.564, 3GPP TS 29.572, 3GPP TS 29.591
 */
struct Supi {
    std::string value;
    bool operator==(const Supi& other) const { return value == other.value; }
};

/**
 * @brief Represents the Generic Public Subscription Identifier (GPSI) for a UE.
 * @source 3GPP TS 23.501, 3GPP TS 29.505, 3GPP TS 29.514, 3GPP TS 29.517, 3GPP TS 29.534, 3GPP TS 29.557, 3GPP TS 29.564, 3GPP TS 29.572, 3GPP TS 29.591
 */
struct Gpsi {
    std::string value;
    bool operator==(const Gpsi& other) const { return value == other.value; }
};

/**
 * @brief Represents a User Location, which can be a combination of different access locations.
 * @source 3GPP TS 29.519, 3GPP TS 29.571 (UserLocation data type referenced by)
 */
struct UserLocation {
    std::optional<std::string> location_area_5g; // Represents a user location area when the UE is attached to 5G
    std::vector<Tac> tai_list; // List of Tracking Areas where the service is allowed
    std::optional<std::string> geographical_area; // Represents a geographical area (e.g., coordinates)
    bool operator==(const UserLocation& other) const {
        return location_area_5g == other.location_area_5g &&
               tai_list == other.tai_list &&
               geographical_area == other.geographical_area;
    }
};

/**
 * @brief Placeholder for UE Location Information, including UserLocation and time zone.
 * @source Based on 3GPP TS 29.519, 3GPP TS 29.571
 */
struct UeLocationInfo {
    UserLocation user_location; // The core user location details
    std::optional<std::string> time_zone;
    // Add other relevant location details as needed
    bool operator==(const UeLocationInfo& other) const {
        return user_location == other.user_location && time_zone == other.time_zone;
    }
};


/**
 * @brief Represents a traffic route to/from a Data Network Access Identifier (DNAI).
 * @source 3GPP TS 29.519, 3GPP TS 29.571 (RouteToLocation data type)
 */
struct RouteToLocation {
    std::string dnai_identifier; // Identifies a Data Network Access Identifier (DNAI)
    // Additional routing details like traffic filters, etc., could be added here if needed,
    // but not strictly required for the combined UE status/IP focus.
    bool operator==(const RouteToLocation& other) const { return dnai_identifier == other.dnai_identifier; }
};

/**
 * @brief Contains the ProSe Discovery UE ID and its validity timer.
 * @source 3GPP TS 29.534
 */
struct PduidInformation {
    DateTime expiry; // Expiration time of validity of UE policies for 5G ProSe direct discovery
    std::string pduid; // Contains the PDUID
    bool operator==(const PduidInformation& other) const {
        return expiry == other.expiry && pduid == other.pduid;
    }
};

/**
 * @brief Represents a Service Area Coverage Information.
 * @source 3GPP TS 29.534, 3GPP TS 29.519
 */
struct ServiceAreaCoverageInfo {
    std::vector<Tac> tac_list; // List of Tracking Areas where the service is allowed
    std::optional<PlmnIdNid> serving_network; // Serving Network (PLMN ID and NID)
    bool operator==(const ServiceAreaCoverageInfo& other) const {
        return tac_list == other.tac_list && serving_network == other.serving_network;
    }
};


/**
 * @brief Represents a Group Identifier, which can be an internal or external group ID.
 * @source 3GPP TS 29.519, 3GPP TS 29.571
 */
struct GroupId {
    std::string value; // Represents the group identifier, can be internal or external
    bool operator==(const GroupId& other) const { return value == other.value; }
};

/**
 * @brief Represents a Traffic Correlation Information.
 * @source 3GPP TS 29.519, 3GPP TS 29.571
 */
struct TrafficCorrelationInfo {
    std::string correlation_id; // Unique identifier for traffic correlation
    std::optional<DateTime> correlation_timestamp; // Timestamp of the correlation information
    std::vector<RouteToLocation> routing_info; // List of routing information for the traffic
    bool operator==(const TrafficCorrelationInfo& other) const {
        return correlation_id == other.correlation_id &&
               correlation_timestamp == other.correlation_timestamp &&
               routing_info == other.routing_info;
    }
};

// Flow Description Types
/**
 * @brief Represents a flow description for IP traffic.
 * @source 3GPP TS 29.519, 3GPP TS 29.571
 */
struct FlowDescription {
    std::optional<std::string> value; // Represents the flow description value, can be an IP address, port, or other identifiers
    bool operator==(const FlowDescription& other) const {
        return value == other.value;
    }
};

/**
 * @brief Represents a flow description for Ethernet traffic.
 * @source 3GPP TS 29.519, 3GPP TS 29.571
 */
struct EthFlowDescription {
    std::optional<std::string> value; // Represents the Ethernet flow description value, can be a MAC address or other identifiers
    bool operator==(const EthFlowDescription& other) const {
        return value == other.value;
    }
};


// AF Request and Response Types

/**
 * @brief Represents UE and Location identification information used in AF requests
 */
struct UeIdentifiers {
    /**
     * @brief UE's **IPv4 address**.
     * **How it's used**:
     * - Used as an **identifier of the PDU session** when reporting `TSC user plane node information` to the AF.
     * - Can be used in `TrafficInfluData` to identify the UE for traffic influence.
     * **When it's used**:
     * - In AF requests to identify the specific UE for which traffic influence or QoS policies are being requested.
     */
    std::optional<Ipv4Addr> ueIpV4Addr;

    /**
     * @brief UE's **IPv6 address or prefix**.
     * **How it's used**:
     * - Similar to `ueIpV4Addr`, for IPv6 PDU sessions.
     * - Can be used in `TrafficInfluData` to identify the UE for traffic influence.
     * **When it's used**:
     * - In AF requests to identify the specific UE for which traffic influence or QoS policies are being requested.
     */
    std::optional<Ipv6Addr> ueIpV6Addr;

    /**
     * @brief UE's **MAC address**.
     * **How it's used**:
     * - A change in the UE MAC address can trigger a `UE_MAC_CH` policy control request trigger to the PCF.
     * **When it's used**:
     * - The SMF reports UE MAC addresses to the PCF. The AF does not directly provide this, but may be an implicit identifier in some contexts.
     */
    std::optional<MacAddr48> ueMacAddr;

    /**
     * @brief **Generic Public Subscription Identifier (GPSI)**.
     * **How it's used**:
     * - Identifies a UE for targeting for `AF event exposure`.
     * - A `required` field in `DispersionCollection` for event exposure.
     * - Used in `AppAmContextData` in `Npcf_AMPolicyAuthorization`.
     * **When it's used**:
     * - In AF requests when referring to a UE or group of UEs using a public identifier, particularly for event subscriptions.
     */
    std::optional<std::string> gpsi;

    /**
     * @brief **Subscription Permanent Identifier (SUPI)**.
     * **How it's used**:
     * - Uniquely identifies a user subscription.
     * - Used for `SUPI based paging`.
     * - A `required` field in `AppAmContextData`.
     * - Used in `AfRequestedQosData` and `TrafficInfluData`.
     * **When it's used**:
     * - In AF requests to identify the subscriber for policy authorization or event exposure.
     */
    std::optional<std::string> supi;

    /**
     * @brief **Internal Group ID** or External Group ID List.
     * **How it's used**:
     * - Identifies a **group of UEs** for which the AF request applies.
     * - A `GROUP_ID_LIST_CHG` trigger indicates that the `UE Internal Group Identifier(s)` has changed.
     * - Used in `TrafficInfluData` and `AfRequestedQosData`.
     * **When it's used**:
     * - In AF requests when targeting policy or QoS decisions for a specific group of UEs.
     */
    std::optional<GroupId> interGroupId;
};


/**
 * @brief Represents network identifier parameters used in AF requests
 */
struct NetworkIdentifiers {
    /**
     * @brief Data Network Name (`DNN`).
     * **How it's used**:
     * - Identifies the data network to which the PDU session is connected.
     * - Used to define target traffic for traffic influence.
     * - Can be included in `PdtqData`.
     * **When it's used**:
     * - In AF requests to specify the network slice associated with the application traffic.
     */
    std::optional<std::string> dnn;

    /**
     * @brief Single Network Slice Selection Assistance Information (`S-NSSAI`).
     * **How it's used**:
     * - Identifies the Network Slice to which the PDU session belongs.
     * - Used in combination with DNN to identify target traffic for AF requests.
     * - Used for network slice usage control.
     * **When it's used**:
     * - In AF requests to specify the network slice associated with the application traffic.
     */
    std::optional<Snssai> snssai;

    /**
     * @brief Network Slice Instance Identifier (`NSI ID`).
     * **How it's used**:
     * - Identifies a specific instance of a Network Slice.
     * - The NSSF may return NSI ID(s) to be associated with Network Slice instance(s).
     * **When it's used**:
     * - In network slice selection responses from the NSSF.
     * - In NF profiles registered with NRF.
     */
    std::optional<std::string> nsiId;

    /**
     * @brief Public Land Mobile Network ID (`PLMN ID`).
     * **How it's used**:
     * - Uniquely identifies a Public Land Mobile Network.
     * - Used for network selection, mobility management, and roaming.
     * **When it's used**:
     * - In network selection procedures and handovers.
     * - For inter-PLMN NF communication.
     */
    std::optional<std::string> plmnId;

    /**
     * @brief Network identifier (`NID`).
     * **How it's used**:
     * - Used with PLMN ID to identify a Stand-alone Non-Public Network (SNPN).
     * - Part of NF profile for SNPNs in NRF.
     * **When it's used**:
     * - When a UE performs Registration or Service Request to an SNPN.
     * - During broadcasting of available SNPNs.
     */
    std::optional<std::string> nid;

    /**
     * @brief External Group Identifier (`ExtGroupId`).
     * **How it's used**:
     * - AF requests can target groups of UEs identified by External Group Identifier(s).
     * - Used for sending MT NIDD messages to UE groups.
     * **When it's used**:
     * - When an AF specifies a group of target UEs for traffic influence or event exposure via NEF.
     */
    std::optional<std::string> extGroupId;

    /**
     * @brief SNPN ID (`SNPN ID`).
     * **How it's used**:
     * - Uniquely identifies a Stand-alone Non-Public Network.
     * - Used for SNPN selection, mobility management, and security.
     * **When it's used**:
     * - When a UE attempts to access an SNPN.
     * - In network sharing scenarios involving SNPNs.
     */
    std::optional<std::string> snpnId;
};

/**
 * @brief Represents traffic filtering information (service data flow filters) used in AF requests
 */
struct TrafficFilteringInformation {
    /**
     * @brief Identifies **IP packet filters** (`FlowInfo` or `FlowDescription`).
     * The AF provides this information to the PCF to enable traffic detection and QoS parameter mapping.
     * **How it's used**:
     * - The filters specify parameters like **source/destination IP addresses, port numbers, protocol, ToS/TC, SPI, Flow Label, and direction**.
     * - The AF provides these filters within the `medSubComps` attribute (using `fDescs` elements).
     * - The **PCF uses these to generate PCC Rules** with `IP Packet Filter sets`.
     * - The **SMF then provisions these filters to the UPF as `SDF Filter(s)` within Packet Detection Rules (PDRs)** to instruct the UPF to detect specific traffic.
     * **When it's used**:
     * - During **Npcf_PolicyAuthorization_Create** (initial session establishment) and **Npcf_PolicyAuthorization_Update** (session modification) service operations.
     * - When the AF needs to specify which IP traffic flows are subject to the requested QoS or traffic routing policies.
     */
    std::optional<std::vector<FlowDescription>> ipTrafficFilters;

    /**
     * @brief Identifies **Ethernet packet filters** (`EthFlowDescription`).
     * Similar to IP filters, these enable the AF to specify Ethernet traffic flows for QoS and policy control.
     * **How it's used**:
     * - The filters specify parameters like **source/destination MAC address, Ethertype, VLAN tags (C-TAG/S-TAG), PCP/DEI fields**, and can optionally include an IP Packet Filter Set if the Ethertype indicates an IPv4/IPv6 payload.
     * - The AF provides these filters within the `medSubComps` attribute (using `ethfDescs` elements).
     * - The **PCF uses these to generate PCC Rules** with `Ethernet Packet Filter sets`.
     * - The **SMF then provisions these filters to the UPF as `Ethernet Packet Filter(s)` within Packet Detection Rules (PDRs)** to detect specific Ethernet traffic.
     * **When it's used**:
     * - During **Npcf_PolicyAuthorization_Create** (initial session establishment) and **Npcf_PolicyAuthorization_Update** (session modification) service operations.
     * - When the AF needs to apply QoS or traffic routing policies to specific Ethernet traffic flows, especially for Ethernet PDU sessions.
     */
    std::optional<std::vector<EthFlowDescription>> ethernetTrafficFilters;
};

// --- Hash specializations for custom structs to use with std::unordered_map ---
namespace std {

// Helper for hashing optional values
template<typename T>
size_t hash_optional(const std::optional<T>& opt) {
    return opt ? std::hash<T>{}(*opt) : 0;
}

// Helper for combining hashes (a common pattern similar to boost::hash_combine)
inline void hash_combine(std::size_t& seed, const std::size_t& v) {
    seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Implementations for basic string-wrapped types
template<> struct hash<Supi> { size_t operator()(const Supi& s) const { return hash<string>{}(s.value); } };
template<> struct hash<Gpsi> { size_t operator()(const Gpsi& g) const { return hash<string>{}(g.value); } };
template<> struct hash<Dnn> { size_t operator()(const Dnn& d) const { return hash<string>{}(d.value); } };
template<> struct hash<Ipv4Addr> { size_t operator()(const Ipv4Addr& i) const { return hash<string>{}(i.value); } };
template<> struct hash<Ipv6Addr> { size_t operator()(const Ipv6Addr& i) const { return hash<string>{}(i.value); } };
template<> struct hash<Ipv6Prefix> { size_t operator()(const Ipv6Prefix& i) const { return hash<string>{}(i.value); } };
template<> struct hash<MacAddr48> { size_t operator()(const MacAddr48& m) const { return hash<string>{}(m.value); } };
template<> struct hash<DateTime> { size_t operator()(const DateTime& dt) const { return hash<string>{}(dt.value); } };
template<> struct hash<Uri> { size_t operator()(const Uri& u) const { return hash<string>{}(u.value); } };
template<> struct hash<AccessType> { size_t operator()(const AccessType& at) const { return hash<string>{}(at.value); } };
template<> struct hash<RatType> { size_t operator()(const RatType& rt) const { return hash<string>{}(rt.value); } };
template<> struct hash<Uinteger> { size_t operator()(const Uinteger& ui) const { return hash<unsigned int>{}(ui.value); } };
template<> struct hash<Tac> { size_t operator()(const Tac& tac) const { return hash<string>{}(tac.value); } };

} // namespace std


#endif // AF_CORE_MODEL_COMMON_H