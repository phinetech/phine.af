#include "models/ue_state.h"
#include "ue_state_manager.h"
#include <algorithm>
#include <chrono>

UeStateManager::UeStateManager()
    : event_dispatcher_(nullptr) // Initialize pointer or other members as needed
{
    // Initialize other members here if necessary
}

UeStateManager::UeStateManager(std::shared_ptr<af::core::events::EventDispatcher> dispatcher) {
    event_dispatcher_ = dispatcher;
}

// === Core State Management Methods ===

void UeStateManager::update_ue_state(const AfUeSubscriptionState& ue_state) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    
    // Call internal method directly to avoid recursive locking
    std::string key_string = ue_state.ue_key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    
    if (it != ue_states_by_key_.end()) {
        // Update existing state
        remove_ue_indices(it->second);
        it->second = ue_state;
        it->second.last_updated = std::chrono::system_clock::now();
        add_ue_indices(it->second);
    } else {
        // Insert new state
        auto [inserted_it, success] = ue_states_by_key_.emplace(key_string, ue_state);
        inserted_it->second.last_updated = std::chrono::system_clock::now();
        add_ue_indices(inserted_it->second);
    }
}

void UeStateManager::update_ue_state_internal(const AfUeSubscriptionState& ue_state) {
    std::string key_string = ue_state.ue_key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    
    if (it != ue_states_by_key_.end()) {
        // Update existing state
        remove_ue_indices(it->second);
        it->second = ue_state;
        it->second.last_updated = std::chrono::system_clock::now();
        add_ue_indices(it->second);
    } else {
        // Insert new state
        auto [inserted_it, success] = ue_states_by_key_.emplace(key_string, ue_state);
        inserted_it->second.last_updated = std::chrono::system_clock::now();
        add_ue_indices(inserted_it->second);
    }
}

AfUeSubscriptionState& UeStateManager::create_or_update_provisional_ue_state(
    const UeKey& key, 
    const std::optional<AfUeSubscriptionState>& initial_data) {
    
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    
    if (it != ue_states_by_key_.end()) {
        // Update existing state
        if (initial_data) {
            // Merge with existing data, preserving key and resolution state
            AfUeSubscriptionState merged = *initial_data;
            merged.ue_key = it->second.ue_key;
            merged.resolution_state = it->second.resolution_state;
            merged.supi = it->second.supi; // Preserve resolved SUPI if any
            merged.last_updated = std::chrono::system_clock::now();;
            
            remove_ue_indices(it->second);
            it->second = merged;
            add_ue_indices(it->second);
        } else {
            it->second.last_updated = std::chrono::system_clock::now();
        }
        return it->second;
    } else {
        // Create new provisional state
        AfUeSubscriptionState new_state = initial_data ? *initial_data : AfUeSubscriptionState(key);
        new_state.ue_key = key;
        if (!initial_data) {
            new_state.resolution_state = AfUeSubscriptionState::ResolutionState::PROVISIONAL;
        }
        
        auto [inserted_it, success] = ue_states_by_key_.emplace(key_string, std::move(new_state));
        add_ue_indices(inserted_it->second);
        return inserted_it->second;
    }
}

bool UeStateManager::promote_ue_state_to_resolved(const UeKey& current_key, const Supi& supi) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    
    std::string current_key_string = current_key.get_primary_key();
    auto it = ue_states_by_key_.find(current_key_string);
    
    if (it == ue_states_by_key_.end()) {
        return false; // UE state not found
    }
    
    // Check if we already have a SUPI-based state
    UeKey supi_key(supi);
    std::string supi_key_string = supi_key.get_primary_key();
    
    if (current_key_string == supi_key_string) {
        // Already SUPI-based, just update resolution
        it->second.resolve_supi(supi);
        return true;
    }
    
    auto supi_it = ue_states_by_key_.find(supi_key_string);
    if (supi_it != ue_states_by_key_.end()) {
        // Merge with existing SUPI-based state
        return merge_ue_states(supi_key, current_key);
    }
    
    // Promote current state to SUPI-based
    remove_ue_indices(it->second);
    
    AfUeSubscriptionState promoted_state = std::move(it->second);
    promoted_state.resolve_supi(supi);
    promoted_state.ue_key = supi_key;
    
    ue_states_by_key_.erase(it);
    auto [new_it, success] = ue_states_by_key_.emplace(supi_key_string, std::move(promoted_state));
    add_ue_indices(new_it->second);
    
    return success;
}

bool UeStateManager::merge_ue_states(const UeKey& primary_key, const UeKey& secondary_key) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    
    std::string primary_key_string = primary_key.get_primary_key();
    std::string secondary_key_string = secondary_key.get_primary_key();
    
    auto primary_it = ue_states_by_key_.find(primary_key_string);
    auto secondary_it = ue_states_by_key_.find(secondary_key_string);
    
    if (primary_it == ue_states_by_key_.end() || secondary_it == ue_states_by_key_.end()) {
        return false;
    }
    
    // Merge secondary into primary
    remove_ue_indices(primary_it->second);
    remove_ue_indices(secondary_it->second);
    
    AfUeSubscriptionState& primary_state = primary_it->second;
    const AfUeSubscriptionState& secondary_state = secondary_it->second;
    
    // Merge known identifiers
    for (const auto& ipv4 : secondary_state.known_ipv4_addresses) {
        primary_state.add_known_identifier("ipv4", ipv4);
    }
    for (const auto& ipv6 : secondary_state.known_ipv6_addresses) {
        primary_state.add_known_identifier("ipv6", ipv6);
    }
    for (const auto& gpsi : secondary_state.known_gpsi_values) {
        primary_state.add_known_identifier("gpsi", gpsi);
    }
    
    // Merge PDU sessions (avoiding duplicates)
    for (const auto& pdu : secondary_state.pdu_sessions) {
        auto existing = std::find_if(primary_state.pdu_sessions.begin(), primary_state.pdu_sessions.end(),
            [&](const PduSessionData& existing_pdu) {
                return existing_pdu.pdu_session_id == pdu.pdu_session_id;
            });
        if (existing == primary_state.pdu_sessions.end()) {
            primary_state.pdu_sessions.push_back(pdu);
        }
    }
    
    primary_state.last_updated = std::chrono::system_clock::now();;
    
    // Remove secondary state and add updated indices for primary
    ue_states_by_key_.erase(secondary_it);
    add_ue_indices(primary_state);
    
    return true;
}

bool UeStateManager::remove_ue_state(const UeKey& key) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        remove_ue_indices(it->second); // Clean up secondary indices
        ue_states_by_key_.erase(it);   // Remove from primary storage
        return true;
    }
    return false;
}

bool UeStateManager::remove_ue_state(const Supi& supi) {
    return remove_ue_state(UeKey(supi));
}

// === Search Functions ===

std::vector<AfUeSubscriptionState> UeStateManager::get_all_ue_states() const {
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    std::vector<AfUeSubscriptionState> all_states;
    all_states.reserve(ue_states_by_key_.size());
    
    for (const auto& pair : ue_states_by_key_) {
        all_states.push_back(pair.second); // Copy each state
    }
    
    return all_states;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_key(const UeKey& key) const {
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        return it->second; // Return a copy
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_supi(const Supi& supi) const {
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    auto it = key_by_supi_.find(supi.value);
    if (it != key_by_supi_.end()) {
        // Avoid recursive locking by accessing directly
        auto ue_it = ue_states_by_key_.find(it->second);
        if (ue_it != ue_states_by_key_.end()) {
            return ue_it->second;
        }
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_gpsi(const Gpsi& gpsi) const {
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    auto it = key_by_gpsi_.find(gpsi.value);
    if (it != key_by_gpsi_.end()) {
        return get_ue_state_by_key(UeKey(gpsi));
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_ipv4(const Ipv4Addr& ipv4_addr) const {
    std::lock_guard<std::shared_mutex> lock(rw_mtx_);
    auto it = key_by_ipv4_addr_.find(ipv4_addr.value);
    if (it != key_by_ipv4_addr_.end()) {
        return get_ue_state_by_key(UeKey(ipv4_addr));
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_ipv6_addr(const Ipv6Addr& ipv6_addr) const {
    std::lock_guard<std::shared_mutex> lock(rw_mtx_);
    auto it = key_by_ipv6_addr_.find(ipv6_addr.value);
    if (it != key_by_ipv6_addr_.end()) {
        return get_ue_state_by_key(UeKey(ipv6_addr));
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_ipv6_prefix(const Ipv6Prefix& ipv6_prefix) const {
    std::lock_guard<std::shared_mutex> lock(rw_mtx_);
    std::string key = ipv6_prefix.value; // Use the full prefix string as key
    auto it = key_by_ipv6_prefix_.find(key);
    if (it != key_by_ipv6_prefix_.end()) {
        return get_ue_state_by_key(UeKey(ipv6_prefix));
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_mac_addr(const MacAddr48& mac_addr) const {
    std::lock_guard<std::shared_mutex> lock(rw_mtx_);
    // MAC addresses are typically associated with PDU sessions, so we need to search through PDU session data
    for (const auto& [key_str, ue_state] : ue_states_by_key_) {
        for (const auto& pdu_session : ue_state.pdu_sessions) {
            if (pdu_session.ue_mac_address && pdu_session.ue_mac_address->value == mac_addr.value) {
                return ue_state;
            }
        }
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_pdu_session_id(const std::string& pdu_session_id) const {
    std::lock_guard<std::shared_mutex> lock(rw_mtx_);
    // PDU sessions are part of UE state, so we need to search through all UE states
    for (const auto& [key_str, ue_state] : ue_states_by_key_) {
        for (const auto& pdu_session : ue_state.pdu_sessions) {
            if (pdu_session.pdu_session_id == pdu_session_id) {
                return ue_state;
            }
        }
    }
    return std::nullopt;
}

// === Update Functions Implementation ===

bool UeStateManager::update_ue_gpsi(const UeKey& key, const Gpsi& new_gpsi) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        // Remove old GPSI index if it existed
        if (it->second.gpsi) {
            key_by_gpsi_.erase(it->second.gpsi->value);
        }
        // Update GPSI in the main state
        it->second.gpsi = new_gpsi;
        it->second.last_updated = std::chrono::system_clock::now();
        // Add new GPSI index
        key_by_gpsi_[new_gpsi.value] = key_string;
        return true;
    }
    return false;
}

bool UeStateManager::update_ue_gpsi(const Supi& supi, const Gpsi& new_gpsi) {
    return update_ue_gpsi(UeKey(supi), new_gpsi);
}

bool UeStateManager::update_ue_location_info(const UeKey& key, const UeLocationInfo& new_location_info, const std::chrono::system_clock::time_point& timestamp) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        it->second.location_info = new_location_info;
        it->second.location_timestamp = timestamp;
        it->second.last_updated = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

bool UeStateManager::update_ue_access_mobility_data(const UeKey& key, const UeAccessMobilityData& new_access_mobility_data) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        it->second.access_mobility_data = new_access_mobility_data;
        it->second.last_updated = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

bool UeStateManager::update_ue_access_mobility_data(const Supi& supi, const UeAccessMobilityData& new_access_mobility_data) {
    return update_ue_access_mobility_data(UeKey(supi), new_access_mobility_data);
}

bool UeStateManager::add_or_update_pdu_session(const UeKey& key, const PduSessionData& pdu_session_data) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        // Remove old indices for this PDU session if it exists
        for (size_t i = 0; i < it->second.pdu_sessions.size(); ++i) {
            if (it->second.pdu_sessions[i].pdu_session_id == pdu_session_data.pdu_session_id) {
                // PDU Session exists, update it
                remove_pdu_session_indices(key_string, it->second.pdu_sessions[i]);
                it->second.pdu_sessions[i] = pdu_session_data; // Update data
                add_pdu_session_indices(key_string, pdu_session_data); // Add new indices
                it->second.last_updated = std::chrono::system_clock::now();
                return true;
            }
        }
        // PDU Session does not exist, add it
        it->second.pdu_sessions.push_back(pdu_session_data);
        add_pdu_session_indices(key_string, pdu_session_data);
        it->second.last_updated = std::chrono::system_clock::now();
        return true;
    }
    return false; // UE not found
}

bool UeStateManager::remove_pdu_session(const UeKey& key, const std::string& pdu_session_id) {
    // TODO: When PDU is removed, also change state of any associated QoD sessions to "INACTIVE"

    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        auto& pdu_sessions = it->second.pdu_sessions;
        for (auto session_it = pdu_sessions.begin(); session_it != pdu_sessions.end(); ++session_it) {
            if (session_it->pdu_session_id == pdu_session_id) {
                remove_pdu_session_indices(key_string, *session_it);
                pdu_sessions.erase(session_it);
                
                // Publish PDU Session Terminated Event (only if SUPI is available)
                if (it->second.supi) {
                    af::core::events::PduSessionTerminatedEvent event(*it->second.supi, pdu_session_id);
                    event_dispatcher_->publish(event);
                }
                
                it->second.last_updated = std::chrono::system_clock::now();
                return true;
            }
        }
    }
    return false; // UE or PDU Session not found
}

bool UeStateManager::remove_pdu_session(const Supi& supi, const std::string& pdu_session_id) {
    return remove_pdu_session(UeKey(supi), pdu_session_id);
}

bool UeStateManager::update_ue_policy_data_info(const UeKey& key, const std::string& new_policy_data) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        it->second.ue_policy_data_info = new_policy_data;
        it->second.last_updated = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

bool UeStateManager::update_ue_policy_data_info(const Supi& supi, const std::string& new_policy_data) {
    return update_ue_policy_data_info(UeKey(supi), new_policy_data);
}

bool UeStateManager::update_prose_pduid_information(const UeKey& key, const PduidInformation& new_prose_info) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        it->second.prose_pduid_information = new_prose_info;
        it->second.last_updated = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

bool UeStateManager::update_prose_pduid_information(const Supi& supi, const PduidInformation& new_prose_info) {
    return update_prose_pduid_information(UeKey(supi), new_prose_info);
}

bool UeStateManager::update_time_sync_service_availability(const UeKey& key, bool available_and_capable) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        it->second.time_sync_service_available_and_capable = available_and_capable;
        it->second.last_updated = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

bool UeStateManager::update_time_sync_service_availability(const Supi& supi, bool available_and_capable) {
    return update_time_sync_service_availability(UeKey(supi), available_and_capable);
}

bool UeStateManager::update_service_area_coverage_allowed(const UeKey& key, const std::vector<ServiceAreaCoverageInfo>& new_coverage_info) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        it->second.service_area_coverage_allowed = new_coverage_info;
        it->second.last_updated = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

bool UeStateManager::update_service_area_coverage_allowed(const Supi& supi, const std::vector<ServiceAreaCoverageInfo>& new_coverage_info) {
    return update_service_area_coverage_allowed(UeKey(supi), new_coverage_info);
}

bool UeStateManager::update_high_throughput_desired(const UeKey& key, bool desired) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        it->second.high_throughput_desired_for_ue_traffic = desired;
        it->second.last_updated = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

bool UeStateManager::update_high_throughput_desired(const Supi& supi, bool desired) {
    return update_high_throughput_desired(UeKey(supi), desired);
}

// === Helper Methods Implementation ===

void UeStateManager::add_ue_indices(const AfUeSubscriptionState& ue_state) {
    // Add secondary indices for faster lookups
    if (ue_state.supi) {
        key_by_supi_[ue_state.supi->value] = ue_state.ue_key.get_primary_key();
    }
    if (ue_state.gpsi) {
        key_by_gpsi_[ue_state.gpsi->value] = ue_state.ue_key.get_primary_key();
    }
    
    // Add indices from PDU session data
    for (const auto& pdu_session : ue_state.pdu_sessions) {
        if (pdu_session.ue_ipv4_address) {
            key_by_ipv4_addr_[pdu_session.ue_ipv4_address->value] = ue_state.ue_key.get_primary_key();
        }
        for (const auto& ipv6_prefix : pdu_session.ue_ipv6_prefixes) {
            key_by_ipv6_prefix_[ipv6_prefix.value] = ue_state.ue_key.get_primary_key();
        }
        for (const auto& ipv6_addr : pdu_session.ue_ipv6_addresses) {
            key_by_ipv6_addr_[ipv6_addr.value] = ue_state.ue_key.get_primary_key();
        }
    }
}

void UeStateManager::remove_ue_indices(const AfUeSubscriptionState& ue_state) {
    // Remove secondary indices
    if (ue_state.supi) {
        key_by_supi_.erase(ue_state.supi->value);
    }
    if (ue_state.gpsi) {
        key_by_gpsi_.erase(ue_state.gpsi->value);
    }
    
    // Remove indices from PDU session data
    for (const auto& pdu_session : ue_state.pdu_sessions) {
        if (pdu_session.ue_ipv4_address) {
            key_by_ipv4_addr_.erase(pdu_session.ue_ipv4_address->value);
        }
        for (const auto& ipv6_prefix : pdu_session.ue_ipv6_prefixes) {
            key_by_ipv6_prefix_.erase(ipv6_prefix.value);
        }
        for (const auto& ipv6_addr : pdu_session.ue_ipv6_addresses) {
            key_by_ipv6_addr_.erase(ipv6_addr.value);
        }
    }
}

void UeStateManager::add_pdu_session_indices(const std::string& key_string, const PduSessionData& pdu_session) {

    if (pdu_session.ue_ipv4_address) {
        key_by_ipv4_addr_[pdu_session.ue_ipv4_address->value] = key_string;
    }
    for (const auto& ipv6_prefix : pdu_session.ue_ipv6_prefixes) {
        key_by_ipv6_prefix_[ipv6_prefix.value] = key_string;
    }
    for (const auto& ipv6_addr : pdu_session.ue_ipv6_addresses) {
        key_by_ipv6_addr_[ipv6_addr.value] = key_string;
    }
}

void UeStateManager::remove_pdu_session_indices(const std::string& key_string, const PduSessionData& pdu_session) {
    // Only erase if the entry in the index map truly points to the UE key we are modifying.
    // This handles cases where an IP might theoretically be reused after a UE de-registers,
    // preventing accidental removal of another UE's valid index.

    if (pdu_session.ue_ipv4_address) {
        auto it_ipv4 = key_by_ipv4_addr_.find(pdu_session.ue_ipv4_address->value);
        if (it_ipv4 != key_by_ipv4_addr_.end() && it_ipv4->second == key_string) {
            key_by_ipv4_addr_.erase(it_ipv4);
        }
    }
    for (const auto& ipv6_prefix : pdu_session.ue_ipv6_prefixes) {
        auto it_ipv6_prefix = key_by_ipv6_prefix_.find(ipv6_prefix.value);
        if (it_ipv6_prefix != key_by_ipv6_prefix_.end() && it_ipv6_prefix->second == key_string) {
            key_by_ipv6_prefix_.erase(it_ipv6_prefix);
        }
    }
    for (const auto& ipv6_addr : pdu_session.ue_ipv6_addresses) {
        auto it_ipv6_addr = key_by_ipv6_addr_.find(ipv6_addr.value);
        if (it_ipv6_addr != key_by_ipv6_addr_.end() && it_ipv6_addr->second == key_string) {
            key_by_ipv6_addr_.erase(it_ipv6_addr);
        }
    }
}

bool UeStateManager::add_qod_session_to_pdu_session(const UeKey& key, const std::string& pdu_session_id, const std::string& qod_session_id) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        for (auto& pdu_session : it->second.pdu_sessions) {
            if (pdu_session.pdu_session_id == pdu_session_id) {
                pdu_session.active_qod_session_ids.insert(qod_session_id);
                it->second.last_updated = std::chrono::system_clock::now();
                return true;
            }
        }
    }
    return false; // UE or PDU Session not found
}

bool UeStateManager::add_qod_session_to_pdu_session(const Supi& supi, const std::string& pdu_session_id, const std::string& qod_session_id) {
    return add_qod_session_to_pdu_session(UeKey(supi), pdu_session_id, qod_session_id);
}

bool UeStateManager::remove_qod_session_from_pdu_session(const UeKey& key, const std::string& pdu_session_id, const std::string& qod_session_id) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        for (auto& pdu_session : it->second.pdu_sessions) {
            if (pdu_session.pdu_session_id == pdu_session_id) {
                pdu_session.active_qod_session_ids.erase(qod_session_id);
                it->second.last_updated = std::chrono::system_clock::now();
                return true;
            }
        }
    }
    return false; // UE or PDU Session not found
}

bool UeStateManager::remove_qod_session_from_pdu_session(const Supi& supi, const std::string& pdu_session_id, const std::string& qod_session_id) {
    return remove_qod_session_from_pdu_session(UeKey(supi), pdu_session_id, qod_session_id);
}

bool remove_known_identifier(const std::string& identifier_type, const std::string& identifier_value, AfUeSubscriptionState& ue_state) {
    if (identifier_type == "ipv4") {
        auto it = std::find(ue_state.known_ipv4_addresses.begin(), ue_state.known_ipv4_addresses.end(), identifier_value);
        if (it != ue_state.known_ipv4_addresses.end()) {
            ue_state.known_ipv4_addresses.erase(it);
            return true;
        }
    } else if (identifier_type == "ipv6") {
        auto it = std::find(ue_state.known_ipv6_addresses.begin(), ue_state.known_ipv6_addresses.end(), identifier_value);
        if (it != ue_state.known_ipv6_addresses.end()) {
            ue_state.known_ipv6_addresses.erase(it);
            return true;
        }
    } else if (identifier_type == "gpsi") {
        auto it = std::find(ue_state.known_gpsi_values.begin(), ue_state.known_gpsi_values.end(), identifier_value);
        if (it != ue_state.known_gpsi_values.end()) {
            ue_state.known_gpsi_values.erase(it);
            return true;
        }
    }
    return false; // Identifier type not recognized or value not found
}

bool UeStateManager::remove_non_primary_ue_identifier(const UeKey& key, const std::string& identifier_type, const std::string& identifier_value) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        // Remove the identifier from the UE state
        bool removed = it->second.remove_known_identifier(identifier_type, identifier_value);
        if (removed) {
            // Update indices accordingly
            remove_ue_indices(it->second);
            add_ue_indices(it->second);
            it->second.last_updated = std::chrono::system_clock::now();
            return true;
        }
        return false; // Identifier not found

        // // Remove all known identifiers except the primary key
        // it->second.known_ipv4_addresses.clear();
        // it->second.known_ipv6_addresses.clear();
        // it->second.known_gpsi_values.clear();
        
        // // Remove secondary indices
        // remove_ue_indices(it->second);
        
        // // Re-add primary key index
        // add_ue_indices(it->second);
        
        // it->second.last_updated = std::chrono::system_clock::now();
        // return true;
    }
    return false; // UE not found 
}

std::optional<std::unordered_set<std::string>> UeStateManager::get_qod_sessions_for_pdu_session(const UeKey& key, const std::string& pdu_session_id) const {
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    if (it != ue_states_by_key_.end()) {
        for (const auto& pdu_session : it->second.pdu_sessions) {
            if (pdu_session.pdu_session_id == pdu_session_id) {
                return pdu_session.active_qod_session_ids;
            }
        }
    }
    return std::nullopt; // UE or PDU Session not found
}

std::optional<std::unordered_set<std::string>> UeStateManager::get_qod_sessions_for_pdu_session(const Supi& supi, const std::string& pdu_session_id) const {
    return get_qod_sessions_for_pdu_session(UeKey(supi), pdu_session_id);
}

std::vector<std::string> UeStateManager::get_pdu_sessions_for_qod_session(const std::string& qod_session_id) const {
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    std::vector<std::string> pdu_session_ids;
    for (const auto& ue_pair : ue_states_by_key_) {
        for (const auto& pdu_session : ue_pair.second.pdu_sessions) {
            if (pdu_session.active_qod_session_ids.find(qod_session_id) != pdu_session.active_qod_session_ids.end()) {
                pdu_session_ids.push_back(pdu_session.pdu_session_id);
            }
        }
    }
    return pdu_session_ids;
}

std::vector<AfUeSubscriptionState> UeStateManager::find_ue_states(
    std::optional<AfUeSubscriptionState::ResolutionState> resolution_state,
    std::optional<bool> has_active_sessions) const {
    
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    std::vector<AfUeSubscriptionState> matching_states;
    
    for (const auto& [key_str, ue_state] : ue_states_by_key_) {
        bool matches = true;
        
        // Filter by resolution state if specified
        if (resolution_state && ue_state.resolution_state != *resolution_state) {
            matches = false;
        }
        
        // Filter by active sessions if specified
        if (has_active_sessions && matches) {
            bool has_sessions = false;
            for (const auto& pdu : ue_state.pdu_sessions) {
                if (!pdu.active_qod_session_ids.empty()) {
                    has_sessions = true;
                    break;
                }
            }
            if (*has_active_sessions != has_sessions) {
                matches = false;
            }
        }
        
        if (matches) {
            matching_states.push_back(ue_state);
        }
    }
    
    return matching_states;
}

bool UeStateManager::add_qod_session_to_ue(const UeKey& key, const std::optional<std::string>& pdu_session_id, const std::string& qod_session_id) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    
    // If PDU session ID provided, use existing method
    if (pdu_session_id) {
        lock.unlock(); // Release lock before calling other method to avoid recursive locking
        return add_qod_session_to_pdu_session(key, *pdu_session_id, qod_session_id);
    }
    
    // Otherwise, create provisional UE state if needed and add to first available PDU session
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    
    if (it == ue_states_by_key_.end()) {
        // Create provisional state
        AfUeSubscriptionState new_state(key);
        new_state.resolution_state = AfUeSubscriptionState::ResolutionState::PROVISIONAL;
        
        auto [inserted_it, success] = ue_states_by_key_.emplace(key_string, std::move(new_state));
        add_ue_indices(inserted_it->second);
        it = inserted_it;
    }
    
    // Add to first PDU session or create one if none exists
    if (it->second.pdu_sessions.empty()) {
        // Create a default PDU session
        PduSessionData default_pdu;
        default_pdu.pdu_session_id = "provisional-" + key_string;
        default_pdu.status = "PROVISIONAL";
        it->second.pdu_sessions.push_back(default_pdu);
    }
    
    // Add QoD session to first PDU session
    it->second.pdu_sessions[0].active_qod_session_ids.insert(qod_session_id);
    it->second.last_updated = std::chrono::system_clock::now();
    
    return true;
}

bool UeStateManager::remove_qod_session_from_ue(const UeKey& key, const std::string& qod_session_id) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    std::string key_string = key.get_primary_key();
    auto it = ue_states_by_key_.find(key_string);
    
    if (it == ue_states_by_key_.end()) {
        return false;
    }
    
    bool removed = false;
    for (auto& pdu_session : it->second.pdu_sessions) {
        if (pdu_session.active_qod_session_ids.erase(qod_session_id) > 0) {
            removed = true;
        }
    }
    
    if (removed) {
        it->second.last_updated = std::chrono::system_clock::now();
    }
    
    return removed;
}

void UeStateManager::cleanup_expired_provisional_states(std::chrono::seconds max_age) {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> to_remove;
    
    for (const auto& [key_str, ue_state] : ue_states_by_key_) {
        if (ue_state.resolution_state == AfUeSubscriptionState::ResolutionState::PROVISIONAL) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - ue_state.created_at);
            if (age > max_age) {
                to_remove.push_back(key_str);
            }
        }
    }
    
    for (const auto& key : to_remove) {
        auto it = ue_states_by_key_.find(key);
        if (it != ue_states_by_key_.end()) {
            remove_ue_indices(it->second);
            ue_states_by_key_.erase(it);
        }
    }
}

std::unordered_map<std::string, size_t> UeStateManager::get_state_statistics() const {
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    
    std::unordered_map<std::string, size_t> stats;
    stats["total_states"] = ue_states_by_key_.size();
    stats["resolved_count"] = 0;
    stats["provisional_count"] = 0;
    stats["partial_count"] = 0;
    stats["active_qod_sessions"] = 0;
    
    for (const auto& [key_str, ue_state] : ue_states_by_key_) {
        switch (ue_state.resolution_state) {
            case AfUeSubscriptionState::ResolutionState::RESOLVED:
                stats["resolved_count"]++;
                break;
            case AfUeSubscriptionState::ResolutionState::PROVISIONAL:
                stats["provisional_count"]++;
                break;
            case AfUeSubscriptionState::ResolutionState::PARTIAL:
                stats["partial_count"]++;
                break;
        }
        
        for (const auto& pdu : ue_state.pdu_sessions) {
            stats["active_qod_sessions"] += pdu.active_qod_session_ids.size();
        }
    }
    
    return stats;
}

AfUeSubscriptionState* UeStateManager::get_ue_state_internal(const std::string& key_string) {
    auto it = ue_states_by_key_.find(key_string);
    return (it != ue_states_by_key_.end()) ? &it->second : nullptr;
}

const AfUeSubscriptionState* UeStateManager::get_ue_state_internal(const std::string& key_string) const {
    auto it = ue_states_by_key_.find(key_string);
    return (it != ue_states_by_key_.end()) ? &it->second : nullptr;
}

void UeStateManager::update_indices_for_key_change(const std::string& old_key, const std::string& new_key, const AfUeSubscriptionState& ue_state) {
    // Remove old indices that point to old_key
    if (ue_state.supi) {
        auto it = key_by_supi_.find(ue_state.supi->value);
        if (it != key_by_supi_.end() && it->second == old_key) {
            it->second = new_key;
        }
    }
    
    if (ue_state.gpsi) {
        auto it = key_by_gpsi_.find(ue_state.gpsi->value);
        if (it != key_by_gpsi_.end() && it->second == old_key) {
            it->second = new_key;
        }
    }
    
    // Update PDU session indices
    for (const auto& pdu : ue_state.pdu_sessions) {
        if (pdu.ue_ipv4_address) {
            auto it = key_by_ipv4_addr_.find(pdu.ue_ipv4_address->value);
            if (it != key_by_ipv4_addr_.end() && it->second == old_key) {
                it->second = new_key;
            }
        }
        
        for (const auto& ipv6_addr : pdu.ue_ipv6_addresses) {
            auto it = key_by_ipv6_addr_.find(ipv6_addr.value);
            if (it != key_by_ipv6_addr_.end() && it->second == old_key) {
                it->second = new_key;
            }
        }
        
        for (const auto& ipv6_prefix : pdu.ue_ipv6_prefixes) {
            auto it = key_by_ipv6_prefix_.find(ipv6_prefix.value);
            if (it != key_by_ipv6_prefix_.end() && it->second == old_key) {
                it->second = new_key;
            }
        }
    }
}