#ifndef AF_CORE_MODEL_UE_STATE_H
#define AF_CORE_MODEL_UE_STATE_H


#include <string>
#include <vector>
#include <optional>
#include <functional> // Required for std::hash
#include "common.h" // Include common data types


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
 * IP addresses, and network context. It focuses on the requested
 * combination of UE status and IP address data.
 */
struct AfUeSubscriptionState {
    // **Primary UE Identifiers**
    Supi supi; // Subscription Permanent Identifier
    std::optional<Gpsi> gpsi; // Generic Public Subscription Identifier (GPSI)

    // **UE Network Location and Access Information**
    std::optional<UeLocationInfo> location_info; // UE's last reported location and time zone
    std::optional<DateTime> location_timestamp; // Timestamp when the location was last updated
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

    bool operator==(const AfUeSubscriptionState& other) const {
        return supi == other.supi &&
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