#include "models/ue_state.h"
#include "ue_state_manager.h"

UeStateManager::UeStateManager() = default;

// Public Methods Implementations

void UeStateManager::update_ue_state(const AfUeSubscriptionState& ue_state) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // Find existing UE state to handle updates to secondary indices
    auto it = ue_states_by_supi_.find(ue_state.supi.value);
    if (it != ue_states_by_supi_.end()) {
        // UE exists, remove old indices before updating the state
        remove_ue_indices(it->second);
    }

    // Add/update the main UE state in the primary map
    ue_states_by_supi_[ue_state.supi.value] = ue_state;
    
    // Add/update all relevant secondary indices for the new/updated state
    add_ue_indices(ue_state);
}

bool UeStateManager::remove_ue_state(const Supi& supi) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        remove_ue_indices(it->second); // Clean up secondary indices
        ue_states_by_supi_.erase(it);   // Remove from primary storage
        return true;
    }
    return false;
}

// --- Search Functions Implementations ---

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_supi(const Supi& supi) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        return it->second; // Return a copy
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_gpsi(const Gpsi& gpsi) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = supi_by_gpsi_.find(gpsi.value);
    if (it != supi_by_gpsi_.end()) {
        return get_ue_state_by_supi({it->second}); // Use primary lookup
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_ipv4(const Ipv4Addr& ipv4_addr) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = supi_by_ipv4_addr_.find(ipv4_addr.value);
    if (it != supi_by_ipv4_addr_.end()) {
        return get_ue_state_by_supi({it->second});
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_ipv6_addr(const Ipv6Addr& ipv6_addr) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = supi_by_ipv6_addr_.find(ipv6_addr.value);
    if (it != supi_by_ipv6_addr_.end()) {
        return get_ue_state_by_supi({it->second});
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_ipv6_prefix(const Ipv6Prefix& ipv6_prefix) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = supi_by_ipv6_prefix_.find(ipv6_prefix.value);
    if (it != supi_by_ipv6_prefix_.end()) {
        return get_ue_state_by_supi({it->second});
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_mac_addr(const MacAddr48& mac_addr) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = supi_by_mac_addr_.find(mac_addr.value);
    if (it != supi_by_mac_addr_.end()) {
        return get_ue_state_by_supi({it->second});
    }
    return std::nullopt;
}

std::optional<AfUeSubscriptionState> UeStateManager::get_ue_state_by_pdu_session_id(const std::string& pdu_session_id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = supi_by_pdu_session_id_.find(pdu_session_id);
    if (it != supi_by_pdu_session_id_.end()) {
        return get_ue_state_by_supi({it->second});
    }
    return std::nullopt;
}

// --- Specific Update Functions Implementations ---

bool UeStateManager::update_ue_gpsi(const Supi& supi, const Gpsi& new_gpsi) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        // Remove old GPSI index if it existed
        if (it->second.gpsi) {
            supi_by_gpsi_.erase(it->second.gpsi->value);
        }
        // Update GPSI in the main state
        it->second.gpsi = new_gpsi;
        // Add new GPSI index
        supi_by_gpsi_[new_gpsi.value] = supi.value;
        return true;
    }
    return false;
}

bool UeStateManager::update_ue_location_info(const Supi& supi, const UeLocationInfo& new_location_info, const DateTime& timestamp) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        it->second.location_info = new_location_info;
        it->second.location_timestamp = timestamp;
        return true;
    }
    return false;
}

bool UeStateManager::update_ue_access_mobility_data(const Supi& supi, const UeAccessMobilityData& new_access_mobility_data) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        it->second.access_mobility_data = new_access_mobility_data;
        return true;
    }
    return false;
}

bool UeStateManager::add_or_update_pdu_session(const Supi& supi, const PduSessionData& pdu_session_data) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        // Check if PDU Session with this ID already exists for the UE
        for (size_t i = 0; i < it->second.pdu_sessions.size(); ++i) {
            if (it->second.pdu_sessions[i].pdu_session_id == pdu_session_data.pdu_session_id) {
                // PDU Session exists, update it
                remove_pdu_session_indices(supi.value, it->second.pdu_sessions[i]); // Remove old indices
                it->second.pdu_sessions[i] = pdu_session_data; // Update data
                add_pdu_session_indices(supi.value, pdu_session_data); // Add new indices
                return true;
            }
        }
        // PDU Session does not exist, add it
        it->second.pdu_sessions.push_back(pdu_session_data);
        add_pdu_session_indices(supi.value, pdu_session_data);
        return true;
    }
    return false; // UE not found
}

bool UeStateManager::remove_pdu_session(const Supi& supi, const std::string& pdu_session_id) {
    // TODO: WHen pdu is removed, also change state of any associated QoD sessions to "INACTIVE"

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        auto& pdu_sessions = it->second.pdu_sessions;
        for (auto session_it = pdu_sessions.begin(); session_it != pdu_sessions.end(); ++session_it) {
            if (session_it->pdu_session_id == pdu_session_id) {
                remove_pdu_session_indices(supi.value, *session_it);
                pdu_sessions.erase(session_it);
                
                // Publish PDU Session Terminated Event
                af::core::events::PduSessionTerminatedEvent event{supi, pdu_session_id};
                event_dispatcher_->publish(event);
                
                return true;
            }
        }
    }
    return false; // UE or PDU Session not found
}

bool UeStateManager::update_ue_policy_data_info(const Supi& supi, const std::string& new_policy_data) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        it->second.ue_policy_data_info = new_policy_data;
        return true;
    }
    return false;
}

bool UeStateManager::update_prose_pduid_information(const Supi& supi, const PduidInformation& new_prose_info) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        it->second.prose_pduid_information = new_prose_info;
        return true;
    }
    return false;
}

bool UeStateManager::update_time_sync_service_availability(const Supi& supi, bool available_and_capable) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        it->second.time_sync_service_available_and_capable = available_and_capable;
        return true;
    }
    return false;
}

bool UeStateManager::update_service_area_coverage_allowed(const Supi& supi, const std::vector<ServiceAreaCoverageInfo>& new_coverage_info) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        it->second.service_area_coverage_allowed = new_coverage_info;
        return true;
    }
    return false;
}

bool UeStateManager::update_high_throughput_desired(const Supi& supi, bool desired) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        it->second.high_throughput_desired_for_ue_traffic = desired;
        return true;
    }
    return false;
}

// Private Helper Methods Implementations

void UeStateManager::add_ue_indices(const AfUeSubscriptionState& ue_state) {
    if (ue_state.gpsi) {
        supi_by_gpsi_[ue_state.gpsi->value] = ue_state.supi.value;
    }
    for (const auto& pdu_session : ue_state.pdu_sessions) {
        add_pdu_session_indices(ue_state.supi.value, pdu_session);
    }
}

void UeStateManager::remove_ue_indices(const AfUeSubscriptionState& ue_state) {
    if (ue_state.gpsi) {
        supi_by_gpsi_.erase(ue_state.gpsi->value);
    }
    for (const auto& pdu_session : ue_state.pdu_sessions) {
        remove_pdu_session_indices(ue_state.supi.value, pdu_session);
    }
}

void UeStateManager::add_pdu_session_indices(const std::string& supi_value, const PduSessionData& pdu_session) {
    supi_by_pdu_session_id_[pdu_session.pdu_session_id] = supi_value;
    if (pdu_session.ue_ipv4_address) {
        supi_by_ipv4_addr_[pdu_session.ue_ipv4_address->value] = supi_value;
    }
    for (const auto& ipv6_prefix : pdu_session.ue_ipv6_prefixes) {
        supi_by_ipv6_prefix_[ipv6_prefix.value] = supi_value;
    }
    for (const auto& ipv6_addr : pdu_session.ue_ipv6_addresses) {
        supi_by_ipv6_addr_[ipv6_addr.value] = supi_value;
    }
    if (pdu_session.ue_mac_address) {
        supi_by_mac_addr_[pdu_session.ue_mac_address->value] = supi_value;
    }
}

void UeStateManager::remove_pdu_session_indices(const std::string& supi_value, const PduSessionData& pdu_session) {
    // Only erase if the entry in the index map truly points to the SUPI we are modifying.
    // This handles cases where an IP/MAC might theoretically be reused after a UE de-registers,
    // preventing accidental removal of another UE's valid index.
    auto it_pdu_id = supi_by_pdu_session_id_.find(pdu_session.pdu_session_id);
    if (it_pdu_id != supi_by_pdu_session_id_.end() && it_pdu_id->second == supi_value) {
        supi_by_pdu_session_id_.erase(it_pdu_id);
    }

    if (pdu_session.ue_ipv4_address) {
        auto it_ipv4 = supi_by_ipv4_addr_.find(pdu_session.ue_ipv4_address->value);
        if (it_ipv4 != supi_by_ipv4_addr_.end() && it_ipv4->second == supi_value) {
            supi_by_ipv4_addr_.erase(it_ipv4);
        }
    }
    for (const auto& ipv6_prefix : pdu_session.ue_ipv6_prefixes) {
        auto it_ipv6_prefix = supi_by_ipv6_prefix_.find(ipv6_prefix.value);
        if (it_ipv6_prefix != supi_by_ipv6_prefix_.end() && it_ipv6_prefix->second == supi_value) {
            supi_by_ipv6_prefix_.erase(it_ipv6_prefix);
        }
    }
    for (const auto& ipv6_addr : pdu_session.ue_ipv6_addresses) {
        auto it_ipv6_addr = supi_by_ipv6_addr_.find(ipv6_addr.value);
        if (it_ipv6_addr != supi_by_ipv6_addr_.end() && it_ipv6_addr->second == supi_value) {
            supi_by_ipv6_addr_.erase(it_ipv6_addr);
        }
    }
    if (pdu_session.ue_mac_address) {
        auto it_mac = supi_by_mac_addr_.find(pdu_session.ue_mac_address->value);
        if (it_mac != supi_by_mac_addr_.end() && it_mac->second == supi_value) {
            supi_by_mac_addr_.erase(it_mac);
        }
    }
}

void UeStateManager::add_qod_session_to_pdu_session(const Supi& supi, const std::string& pdu_session_id, const std::string& qod_session_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        for (auto& pdu_session : it->second.pdu_sessions) {
            if (pdu_session.pdu_session_id == pdu_session_id) {
                pdu_session.active_qod_session_ids.insert(qod_session_id);
                return true;
            }
        }
    }
    return false; // UE or PDU Session not found
}

void UeStateManager::remove_qod_session_from_pdu_session(const Supi& supi, const std::string& pdu_session_id, const std::string& qod_session_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        for (auto& pdu_session : it->second.pdu_sessions) {
            if (pdu_session.pdu_session_id == pdu_session_id) {
                pdu_session.active_qod_session_ids.erase(qod_session_id);
                return true;
            }
        }
    }
    return false; // UE or PDU Session not found
}

std::optional<std::unordered_set<std::string>> UeStateManager::get_qod_sessions_for_pdu_session(const Supi& supi, const std::string& pdu_session_id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = ue_states_by_supi_.find(supi.value);
    if (it != ue_states_by_supi_.end()) {
        for (const auto& pdu_session : it->second.pdu_sessions) {
            if (pdu_session.pdu_session_id == pdu_session_id) {
                return pdu_session.active_qod_session_ids;
            }
        }
    }
    return std::nullopt; // UE or PDU Session not found
}

std::vector<std::string> UeStateManager::get_pdu_sessions_for_qod_session(const std::string& qod_session_id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> pdu_session_ids;
    for (const auto& ue_pair : ue_states_by_supi_) {
        for (const auto& pdu_session : ue_pair.second.pdu_sessions) {
            if (pdu_session.active_qod_session_ids.find(qod_session_id) != pdu_session.active_qod_session_ids.end()) {
                pdu_session_ids.push_back(pdu_session.pdu_session_id);
            }
        }
    }
    return pdu_session_ids;
}