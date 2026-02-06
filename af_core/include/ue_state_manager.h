#ifndef AF_CORE_UE_STATE_MANAGER_H
#define AF_CORE_UE_STATE_MANAGER_H

#include "models/ue_state.h" 
#include "events/i_event_dispatcher.h"
#include "events/event_dispatcher.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <optional>

/**
 * @brief Manages the state of multiple UEs, providing search and update functionalities.
 * This class is thread-safe and supports flexible UE identification using UeKey system.
 */
class UeStateManager {
public:
    explicit UeStateManager(std::shared_ptr<af::core::events::EventDispatcher> dispatcher);

    UeStateManager();

    // === Core State Management Methods ===

    /**
     * @brief Adds or updates a UE's full state based on a received notification.
     * This is the primary entry point for comprehensive state updates.
     * @param ue_state The complete AfUeSubscriptionState object for the UE.
     */
    void update_ue_state(const AfUeSubscriptionState& ue_state);

    /**
     * @brief Creates or updates a provisional UE state identified by any identifier.
     * Used when SUPI is not available but we need to track the UE (e.g., QoD sessions).
     * @param key The UE key (can be IP, GPSI, etc.)
     * @param initial_data Optional initial data to populate
     * @return Reference to the created/updated UE state
     */
    AfUeSubscriptionState& create_or_update_provisional_ue_state(const UeKey& key, const std::optional<AfUeSubscriptionState>& initial_data = std::nullopt);

    /**
     * @brief Promotes a provisional UE state to resolved when SUPI becomes available.
     * @param current_key Current key identifying the UE
     * @param supi The resolved SUPI
     * @return True if promotion was successful, false otherwise
     */
    bool promote_ue_state_to_resolved(const UeKey& current_key, const Supi& supi);

    /**
     * @brief Merges two UE states that represent the same UE.
     * Used when we discover that two entries are for the same UE.
     * @param primary_key The key to keep (usually SUPI-based)
     * @param secondary_key The key to merge and remove
     * @return True if merge was successful, false otherwise
     */
    bool merge_ue_states(const UeKey& primary_key, const UeKey& secondary_key);

    /**
     * @brief Removes a UE's state and all associated secondary indices.
     * @param key The UE key to remove (can be any valid identifier)
     * @return True if the UE was found and removed, false otherwise
     */
    bool remove_ue_state(const UeKey& key);

    /**
     * @brief Removes a UE's state by SUPI (legacy method for backward compatibility).
     * @param supi The SUPI of the UE to remove.
     * @return True if the UE was found and removed, false otherwise
     */
    bool remove_ue_state(const Supi& supi);

    // === Search Functions ===

    /**
     * @brief Retrieves a UE's state by any valid UE key.
     * @param key The UE key (SUPI, GPSI, IP, etc.)
     * @return An optional containing the AfUeSubscriptionState if found, std::nullopt otherwise.
     */
    std::optional<AfUeSubscriptionState> get_ue_state_by_key(const UeKey& key) const;

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

    /**
     * @brief Retrieve all stored UE states.
     * @return Vector of all UE states
     */
    std::vector<AfUeSubscriptionState> get_all_ue_states() const;

    /**
     * @brief Finds all UE states that match certain criteria.
     * @param resolution_state Optional filter by resolution state
     * @param has_active_sessions Optional filter for UEs with active QoD sessions
     * @return Vector of matching UE states
     */
    std::vector<AfUeSubscriptionState> find_ue_states(
        std::optional<AfUeSubscriptionState::ResolutionState> resolution_state = std::nullopt,
        std::optional<bool> has_active_sessions = std::nullopt) const;

    // === Specific Update Functions (for granular updates) ===

    /**
     * @brief Updates the GPSI for a specific UE.
     * @param key The UE key (can be any valid identifier).
     * @param new_gpsi The new GPSI value.
     * @return True if the UE was found and GPSI updated, false otherwise.
     */
    bool update_ue_gpsi(const UeKey& key, const Gpsi& new_gpsi);
    
    /**
     * @brief Updates the GPSI for a specific UE (legacy SUPI-based method).
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
    bool update_ue_location_info(const UeKey& key, const UeLocationInfo& new_location_info, const std::chrono::system_clock::time_point& timestamp);

    /**
     * @brief Updates the access and mobility data for a specific UE.
     * @param supi The SUPI of the UE.
     * @param new_access_mobility_data The new UeAccessMobilityData object.
     * @return True if the UE was found and access mobility data updated, false otherwise.
     */
    bool update_ue_access_mobility_data(const Supi& supi, const UeAccessMobilityData& new_access_mobility_data);

    /**
     * @brief Updates the access and mobility data for a specific UE.
     * @param key The UE key (can be any valid identifier).
     * @param new_access_mobility_data The new UeAccessMobilityData object.
     * @return True if the UE was found and access mobility data updated, false otherwise.
     */
    bool update_ue_access_mobility_data(const UeKey& key, const UeAccessMobilityData& new_access_mobility_data);

    /**
     * @brief Adds a new PDU Session for a UE, or updates it if it already exists.
     * @param key The UE key (can be any valid identifier).
     * @param pdu_session_data The new PDU Session data.
     * @return True if the PDU session was added/updated, false if UE not found.
     */
    bool add_or_update_pdu_session(const UeKey& key, const PduSessionData& pdu_session_data);

    /**
     * @brief Removes a PDU Session from a UE.
     * @param supi The SUPI of the UE.
     * @param pdu_session_id The ID of the PDU Session to remove.
     * @return True if the PDU session was found and removed, false otherwise.
     */
    bool remove_pdu_session(const Supi& supi, const std::string& pdu_session_id);

    /**
     * @brief Removes a PDU Session from a UE.
     * @param key The UE key (can be any valid identifier).
     * @param pdu_session_id The ID of the PDU Session to remove.
     * @return True if the PDU session was found and removed, false otherwise.
     */
    bool remove_pdu_session(const UeKey& key, const std::string& pdu_session_id);

    /**
     * @brief Remove the non-primary UE key association (e.g., IP, GPSI) from a UE state. Typically used when the identifier is no longer valid.
     * @param key The UE key to remove (can be any valid identifier except SUPI).
     * @param identifier_type The type of identifier to remove (e.g., "IP", "GPSI").
     * @param identifier_value The value of the identifier to remove.
     * @return True if the key association was found and removed, false otherwise.
     */
    bool remove_non_primary_ue_identifier(const UeKey& key, const std::string& identifier_type, const std::string& identifier_value);

    /**
     * @brief Updates the UE Policy Data Information for a specific UE.
     * @param supi The SUPI of the UE.
     * @param new_policy_data The new UE Policy Data.
     * @return True if the UE was found and policy data updated, false otherwise.
     */
    bool update_ue_policy_data_info(const Supi& supi, const std::string& new_policy_data);
    
    /**
     * @brief Updates the UE Policy Data Information for a specific UE.
     * @param key The UE key (can be any valid identifier).
     * @param new_policy_data The new UE Policy Data.
     * @return True if the UE was found and policy data updated, false otherwise.
     */
    bool update_ue_policy_data_info(const UeKey& key, const std::string& new_policy_data);

    /**
     * @brief Updates the ProSe PDUID Information for a specific UE.
     * @param supi The SUPI of the UE.
     * @param new_prose_info The new ProSe PDUID Information.
     * @return True if the UE was found and ProSe info updated, false otherwise.
     */
    bool update_prose_pduid_information(const Supi& supi, const PduidInformation& new_prose_info);

    /**
     * @brief Updates the ProSe PDUID Information for a specific UE.
     * @param key The UE key (can be any valid identifier).
     * @param new_prose_info The new ProSe PDUID Information.
     * @return True if the UE was found and ProSe info updated, false otherwise.
     */
    bool update_prose_pduid_information(const UeKey& key, const PduidInformation& new_prose_info);

    /**
     * @brief Updates the time synchronization service availability status for a UE.
     * @param supi The SUPI of the UE.
     * @param available_and_capable True if time sync service is available and capable, false otherwise.
     * @return True if the UE was found and status updated, false otherwise.
     */
    bool update_time_sync_service_availability(const Supi& supi, bool available_and_capable);

    /**
     * @brief Updates the time synchronization service availability status for a UE.
     * @param key The UE key (can be any valid identifier).
     * @param available_and_capable True if time sync service is available and capable, false otherwise.
     * @return True if the UE was found and status updated, false otherwise.
     */
    bool update_time_sync_service_availability(const UeKey& key, bool available_and_capable);

    /**
     * @brief Updates the list of service area coverage allowed for a UE.
     * @param supi The SUPI of the UE.
     * @param new_coverage_info The new list of ServiceAreaCoverageInfo.
     * @return True if the UE was found and coverage info updated, false otherwise.
     */
    bool update_service_area_coverage_allowed(const Supi& supi, const std::vector<ServiceAreaCoverageInfo>& new_coverage_info);

    /**
     * @brief Updates the list of service area coverage allowed for a UE.
     * @param key The UE key (can be any valid identifier).
     * @param new_coverage_info The new list of ServiceAreaCoverageInfo.
     * @return True if the UE was found and coverage info updated, false otherwise.
     */
    bool update_service_area_coverage_allowed(const UeKey& key, const std::vector<ServiceAreaCoverageInfo>& new_coverage_info);

    /**
     * @brief Updates the high throughput desired status for a UE.
     * @param supi The SUPI of the UE.
     * @param desired True if high throughput is desired, false otherwise.
     * @return True if the UE was found and status updated, false otherwise.
     */
    bool update_high_throughput_desired(const Supi& supi, bool desired);

    /**
     * @brief Updates the high throughput desired status for a UE.
     * @param key The UE key (can be any valid identifier).
     * @param desired True if high throughput is desired, false otherwise.
     * @return True if the UE was found and status updated, false otherwise.
     */
    bool update_high_throughput_desired(const UeKey& key, bool desired);

    // === QoD Session Management ===

    /**
     * @brief Associates a QoD session ID with a specific UE (creates provisional state if needed).
     * @param key The UE key (can be IP if SUPI not available).
     * @param pdu_session_id Optional PDU Session ID to associate with.
     * @param qod_session_id The QoD session ID to add.
     * @return True if the association was made, false otherwise.
     */
    bool add_qod_session_to_ue(const UeKey& key, const std::optional<std::string>& pdu_session_id, const std::string& qod_session_id);

    /**
     * @brief Associates a QoD session ID with a specific PDU session of a UE.
     * @param supi The SUPI of the UE.
     * @param pdu_session_id The PDU Session ID to associate with.
     * @param qod_session_id The QoD session ID to add.
     * @return True if the association was made, false if UE or PDU session not found.
     */
    bool add_qod_session_to_pdu_session(const Supi& supi, const std::string& pdu_session_id, const std::string& qod_session_id);

    /**
     * @brief Associates a QoD session ID with a specific PDU session of a UE.
     * @param key The UE key to associate with.
     * @param pdu_session_id The PDU Session ID to associate with.
     * @param qod_session_id The QoD session ID to add.
     * @return True if the association was made, false if UE or PDU session not found.
     */
    bool add_qod_session_to_pdu_session(const UeKey& key, const std::string& pdu_session_id, const std::string& qod_session_id);

    /**
     * @brief Removes a QoD session ID association from a UE.
     * @param key The UE key.
     * @param qod_session_id The QoD session ID to remove.
     * @return True if the removal was made, false if UE not found.
     */
    bool remove_qod_session_from_ue(const UeKey& key, const std::string& qod_session_id);

    /**
     * @brief Removes a QoD session ID association from a specific PDU session of a UE.
     * @param supi The SUPI of the UE.
     * @param pdu_session_id The PDU Session ID to disassociate from.
     * @param qod_session_id The QoD session ID to remove.
     * @return True if the removal was made, false if UE or PDU session not found.
     */
    bool remove_qod_session_from_pdu_session(const Supi& supi, const std::string& pdu_session_id, const std::string& qod_session_id);

    /**
     * @brief Removes a QoD session ID association from a specific PDU session of a UE.
     * @param key The UE key to disassociate from.
     * @param pdu_session_id The PDU Session ID to disassociate from.
     * @param qod_session_id The QoD session ID to remove.
     * @return True if the removal was made, false if UE or PDU session not found.
     */
    bool remove_qod_session_from_pdu_session(const UeKey& key, const std::string& pdu_session_id, const std::string& qod_session_id);

    /**
     * @brief Retrieves all QoD session IDs associated with a specific PDU session of a UE.
     * @param supi The SUPI of the UE.
     * @param pdu_session_id The PDU Session ID to query.
     * @return An optional set of QoD session IDs if found, std::nullopt if UE or PDU session not found.
     */
    std::optional<std::unordered_set<std::string>> get_qod_sessions_for_pdu_session(const Supi& supi, const std::string& pdu_session_id) const;

    /**
     * @brief Retrieves all QoD session IDs associated with a specific PDU session of a UE.
     * @param key The UE key to query.
     * @param pdu_session_id The PDU Session ID to query.
     * @return An optional set of QoD session IDs if found, std::nullopt if UE or PDU session not found.
     */
    std::optional<std::unordered_set<std::string>> get_qod_sessions_for_pdu_session(const UeKey& key, const std::string& pdu_session_id) const;

    /**
     * @brief Retrive all pdu session ids for a specific qod session id
     * @param qod_session_id The QoD Session ID to query.
     * @return A vector of PDU session IDs associated with the QoD session ID.
     */
    std::vector<std::string> get_pdu_sessions_for_qod_session(const std::string& qod_session_id) const;

    // === Utility Methods ===

    /**
     * @brief Clean up expired provisional UE states that were never resolved.
     * @param max_age Maximum age for provisional states before cleanup
     */
    void cleanup_expired_provisional_states(std::chrono::seconds max_age = std::chrono::seconds(3600));

    /**
     * @brief Get statistics about UE states.
     * @return Map of statistics (resolved_count, provisional_count, etc.)
     */
    std::unordered_map<std::string, size_t> get_state_statistics() const;

private:
    // === Storage Layer ===
    
    // Main storage for UE states, keyed by UeKey primary string for efficient lookup.
    // mutable because const methods need to acquire the mutex.
    mutable std::unordered_map<std::string, AfUeSubscriptionState> ue_states_by_key_;

    // Secondary indices for efficient lookup by other attributes.
    // Maps attribute value (string) to UeKey primary string.
    mutable std::unordered_map<std::string, std::string> key_by_supi_;
    mutable std::unordered_map<std::string, std::string> key_by_gpsi_;
    mutable std::unordered_map<std::string, std::string> key_by_ipv4_addr_;
    mutable std::unordered_map<std::string, std::string> key_by_ipv6_addr_;
    mutable std::unordered_map<std::string, std::string> key_by_ipv6_prefix_;
    mutable std::unordered_map<std::string, std::string> key_by_mac_addr_;
    mutable std::unordered_map<std::string, std::string> key_by_pdu_session_id_;
    
    // Event dispatcher for state change notifications
    std::shared_ptr<af::core::events::EventDispatcher> event_dispatcher_;

    // Reader-writer mutex for thread-safe access to the maps.
    // Allows multiple concurrent readers or one exclusive writer.
    // Note: For high contention scenarios, consider using more granular locks or reader-writer locks.
    mutable std::shared_mutex rw_mtx_;

    // === Helper Methods ===
    
    /**
     * @brief Internal method to get UE state by key string (no mutex lock).
     * @param key_string The key string to look up
     * @return Pointer to UE state if found, nullptr otherwise
     */
    AfUeSubscriptionState* get_ue_state_internal(const std::string& key_string);
    const AfUeSubscriptionState* get_ue_state_internal(const std::string& key_string) const;

    /**
     * @brief Internal method to add/update UE state (no mutex lock).
     * @param ue_state The UE state to add/update
     */
    void update_ue_state_internal(const AfUeSubscriptionState& ue_state);

    /**
     * @brief Helper methods for managing secondary indices
     */
    void add_ue_indices(const AfUeSubscriptionState& ue_state);
    void remove_ue_indices(const AfUeSubscriptionState& ue_state);
    void add_pdu_session_indices(const std::string& key_string, const PduSessionData& pdu_session);
    void remove_pdu_session_indices(const std::string& key_string, const PduSessionData& pdu_session);
    
    /**
     * @brief Update indices when UE state is promoted/merged
     */
    void update_indices_for_key_change(const std::string& old_key, const std::string& new_key, const AfUeSubscriptionState& ue_state);
};

#endif // AF_CORE_UE_STATE_MANAGER_H