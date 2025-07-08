#ifndef AF_CORE_UE_STATE_MANAGER_H
#define AF_CORE_UE_STATE_MANAGER_H

#include "models/ue_state.h" // Include the UE state struct definitions
#include <unordered_map>
#include <mutex>
#include <optional>

/**
 * @brief Manages the state of multiple UEs, providing search and update functionalities.
 * This class is thread-safe.
 */
class UeStateManager {
public:
    UeStateManager(); // Default constructor

    /**
     * @brief Adds or updates a UE's full state based on a received notification.
     * This is the primary entry point for comprehensive state updates.
     * @param ue_state The complete AfUeSubscriptionState object for the UE.
     */
    void update_ue_state(const AfUeSubscriptionState& ue_state);

    /**
     * @brief Removes a UE's state and all associated secondary indices.
     * @param supi The SUPI of the UE to remove.
     * @return True if the UE was found and removed, false otherwise.
     */
    bool remove_ue_state(const Supi& supi);

    // --- Search Functions ---

    /**
     * @brief Retrieves a UE's state by its SUPI.
     * @param supi The SUPI of the UE.
     * @return An optional containing the AfUeSubscriptionState if found, std::nullopt otherwise.
     */
    std::optional<AfUeSubscriptionState> get_ue_state_by_supi(const Supi& supi) const;

    /**
     * @brief Retrieves a UE's state by its GPSI.
     * @param gpsi The GPSI of the UE.
     * @return An optional containing the AfUeSubscriptionState if found, std::nullopt otherwise.
     */
    std::optional<AfUeSubscriptionState> get_ue_state_by_gpsi(const Gpsi& gpsi) const;

    /**
     * @brief Retrieves a UE's state by one of its associated IPv4 Addresses.
     * @param ipv4_addr The IPv4 address.
     * @return An optional containing the AfUeSubscriptionState if found, std::nullopt otherwise.
     */
    std::optional<AfUeSubscriptionState> get_ue_state_by_ipv4(const Ipv4Addr& ipv4_addr) const;

    /**
     * @brief Retrieves a UE's state by one of its associated individual IPv6 Addresses.
     * @param ipv6_addr The IPv6 address.
     * @return An optional containing the AfUeSubscriptionState if found, std::nullopt otherwise.
     */
    std::optional<AfUeSubscriptionState> get_ue_state_by_ipv6_addr(const Ipv6Addr& ipv6_addr) const;

    /**
     * @brief Retrieves a UE's state by one of its associated IPv6 Prefixes.
     * @param ipv6_prefix The IPv6 prefix.
     * @return An optional containing the AfUeSubscriptionState if found, std::nullopt otherwise.
     */
    std::optional<AfUeSubscriptionState> get_ue_state_by_ipv6_prefix(const Ipv6Prefix& ipv6_prefix) const;

    /**
     * @brief Retrieves a UE's state by one of its associated MAC Addresses.
     * @param mac_addr The MAC address.
     * @return An optional containing the AfUeSubscriptionState if found, std::nullopt otherwise.
     */
    std::optional<AfUeSubscriptionState> get_ue_state_by_mac_addr(const MacAddr48& mac_addr) const;

    /**
     * @brief Retrieves a UE's state by one of its PDU Session IDs.
     * @param pdu_session_id The PDU Session Identifier.
     * @return An optional containing the AfUeSubscriptionState if found, std::nullopt otherwise.
     */
    std::optional<AfUeSubscriptionState> get_ue_state_by_pdu_session_id(const std::string& pdu_session_id) const;

    // --- Specific Update Functions (for granular updates) ---

    /**
     * @brief Updates the GPSI for a specific UE.
     * @param supi The SUPI of the UE.
     * @param new_gpsi The new GPSI value.
     * @return True if the UE was found and GPSI updated, false otherwise.
     */
    bool update_ue_gpsi(const Supi& supi, const Gpsi& new_gpsi);

    /**
     * @brief Updates the location information for a specific UE.
     * @param supi The SUPI of the UE.
     * @param new_location_info The new UeLocationInfo object.
     * @param timestamp The timestamp of this location update.
     * @return True if the UE was found and location info updated, false otherwise.
     */
    bool update_ue_location_info(const Supi& supi, const UeLocationInfo& new_location_info, const DateTime& timestamp);

    /**
     * @brief Updates the access and mobility data for a specific UE.
     * @param supi The SUPI of the UE.
     * @param new_access_mobility_data The new UeAccessMobilityData object.
     * @return True if the UE was found and access mobility data updated, false otherwise.
     */
    bool update_ue_access_mobility_data(const Supi& supi, const UeAccessMobilityData& new_access_mobility_data);

    /**
     * @brief Adds a new PDU Session for a UE, or updates it if it already exists.
     * @param supi The SUPI of the UE.
     * @param pdu_session_data The new PDU Session data.
     * @return True if the PDU session was added/updated, false if UE not found.
     */
    bool add_or_update_pdu_session(const Supi& supi, const PduSessionData& pdu_session_data);

    /**
     * @brief Removes a PDU Session from a UE.
     * @param supi The SUPI of the UE.
     * @param pdu_session_id The ID of the PDU Session to remove.
     * @return True if the PDU session was found and removed, false otherwise.
     */
    bool remove_pdu_session(const Supi& supi, const std::string& pdu_session_id);

    /**
     * @brief Updates the UE Policy Data Information for a specific UE.
     * @param supi The SUPI of the UE.
     * @param new_policy_data The new UE Policy Data.
     * @return True if the UE was found and policy data updated, false otherwise.
     */
    bool update_ue_policy_data_info(const Supi& supi, const std::string& new_policy_data);

    /**
     * @brief Updates the ProSe PDUID Information for a specific UE.
     * @param supi The SUPI of the UE.
     * @param new_prose_info The new ProSe PDUID Information.
     * @return True if the UE was found and ProSe info updated, false otherwise.
     */
    bool update_prose_pduid_information(const Supi& supi, const PduidInformation& new_prose_info);

    /**
     * @brief Updates the time synchronization service availability status for a UE.
     * @param supi The SUPI of the UE.
     * @param available_and_capable True if time sync service is available and capable, false otherwise.
     * @return True if the UE was found and status updated, false otherwise.
     */
    bool update_time_sync_service_availability(const Supi& supi, bool available_and_capable);

    /**
     * @brief Updates the list of service area coverage allowed for a UE.
     * @param supi The SUPI of the UE.
     * @param new_coverage_info The new list of ServiceAreaCoverageInfo.
     * @return True if the UE was found and coverage info updated, false otherwise.
     */
    bool update_service_area_coverage_allowed(const Supi& supi, const std::vector<ServiceAreaCoverageInfo>& new_coverage_info);

    /**
     * @brief Updates the high throughput desired status for a UE.
     * @param supi The SUPI of the UE.
     * @param desired True if high throughput is desired, false otherwise.
     * @return True if the UE was found and status updated, false otherwise.
     */
    bool update_high_throughput_desired(const Supi& supi, bool desired);


private:
    // Main storage for UE states, keyed by SUPI string for efficient lookup.
    // mutable because const methods like get_ue_state_by_supi need to acquire the mutex.
    mutable std::unordered_map<std::string, AfUeSubscriptionState> ue_states_by_supi_;

    // Secondary indices for efficient lookup by other attributes.
    // Maps attribute value (string) to SUPI string.
    mutable std::unordered_map<std::string, std::string> supi_by_gpsi_;
    mutable std::unordered_map<std::string, std::string> supi_by_ipv4_addr_;
    mutable std::unordered_map<std::string, std::string> supi_by_ipv6_addr_;
    mutable std::unordered_map<std::string, std::string> supi_by_ipv6_prefix_;
    mutable std::unordered_map<std::string, std::string> supi_by_mac_addr_;
    mutable std::unordered_map<std::string, std::string> supi_by_pdu_session_id_;

    // Mutex for thread-safe access to the maps.
    mutable std::mutex mtx_;

    // Helper methods for managing secondary indices
    void add_ue_indices(const AfUeSubscriptionState& ue_state);
    void remove_ue_indices(const AfUeSubscriptionState& ue_state);
    void add_pdu_session_indices(const std::string& supi_value, const PduSessionData& pdu_session);
    void remove_pdu_session_indices(const std::string& supi_value, const PduSessionData& pdu_session);
};

#endif // AF_CORE_UE_STATE_MANAGER_H