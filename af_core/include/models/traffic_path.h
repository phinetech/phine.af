#ifndef AF_CORE_MODEL_TRAFFIC_PATH_H
#define AF_CORE_MODEL_TRAFFIC_PATH_H

#include <string>
#include <vector>
#include <optional>
#include "common/models/common.h"

/**
 * @brief Represents traffic routing parameters used in AF requests
 */
struct TrafficRoutingParameters {
    /**
     * @brief Indicates the **time interval(s) during which the AF request is to be applied**.
     * **How it's used**:
     * - Specifies the active periods for policy rules.
     * - The PCF can manage temporal invalidity conditions.
     * **When it's used**:
     * - In `AfRoutingRequirement` to define the time-based applicability of traffic routing policies.
     */
    std::optional<std::vector<TemporalValidity>> tempVals;

    /**
     * @brief Specifies **geographical or area of interest** for the request.
     * **How it's used**:
     * - Defines the geographic scope where the routing policy or application functions are relevant.
     * **When it's used**:
     * - In `AfRoutingRequirement` to define location-based applicability of traffic routing policies.
     */
    struct SpatialValidity {
        // ... (details about presenceInfoList)
    };
    std::optional<SpatialValidity> spVal;

    /**
     * @brief Indicates whether **IP address preservation is requested**.
     * **How it's used**:
     * - If `true`, the network attempts to maintain the same IP address for the UE across certain events.
     * **When it's used**:
     * - In `AfRoutingRequirement` to express the AF's preference for IP address stability, important for session continuity.
     */
    std::optional<bool> addrPreserInd;

    /**
     * @brief Indicates whether **simultaneous connectivity should be temporarily maintained** for source and target PSA during edge relocation.
     * **How it's used**:
     * - If `true`, the network maintains connectivity to both source and target PDU Session Anchors (PSAs) for a period.
     * **When it's used**:
     * - In `AfRoutingRequirement` when an application requires seamless mobility with minimal disruption during edge relocation, such as for certain low-latency services.
     */
    std::optional<bool> simConnInd;

    /**
     * @brief Duration for which **simultaneous connectivity should be temporarily maintained** (in seconds).
     * **How it's used**:
     * - Specifies the duration for dual connectivity during edge relocation.
     * **When it's used**:
     * - Used in conjunction with `simConnInd` to define the window for seamless transitions.
     */
    std::optional<DurationSec> simConnTerm;

    /**
     * @brief Contains **EAS (Edge Application Server) IP replacement information**.
     * **How it's used**:
     * - Provides details for replacing an EAS IP address during application relocation or steering.
     * **When it's used**:
     * - In `AfRoutingRequirement` to support edge relocation and traffic steering to different EAS instances.
     */
    std::optional<std::vector<EasIpReplacementInfo>> easIpReplaceInfos;

    /**
     * @brief Indicates if **EAS rediscovery is required**.
     * **How it's used**:
     * - If `true`, it requests the network to re-discover the Edge Application Server.
     * - The AF can trigger this.
     * **When it's used**:
     * - In `AfRoutingRequirement` when an application needs to find a new or optimal EAS instance, for example after a UE mobility event.
     */
    std::optional<bool> easRedisInd;

    /**
     * @brief Specifies the **maximum allowed User Plane (UP) latency**.
     * **How it's used**:
     * - Sets an upper bound on the acceptable latency for user plane traffic.
     * **When it's used**:
     * - In `AfRoutingRequirement` for applications with strict latency requirements.
     */
    std::optional<Uinteger> maxAllowedUpLat;
};

/**
 * @brief Represents Service Function Chaining (SFC) and traffic steering parameters
 */
struct ServiceFunctionChainParameters {
    /**
     * @brief Reference to a **pre-configured Service Function Chain (SFC) for Uplink traffic**.
     * **How it's used**:
     * - Directs uplink user plane traffic through a defined sequence of network functions.
     * - The AF can influence `SFC support`.
     * **When it's used**:
     * - In `AfSfcRequirement` when an application requires specific network functions to be applied to its traffic in a particular order.
     */
    std::optional<std::string> sfcIdUl;

    /**
     * @brief Reference to a **pre-configured Service Function Chain (SFC) for Downlink traffic**.
     * **How it's used**:
     * - Directs downlink user plane traffic through a defined sequence of network functions.
     * - The AF can influence `SFC support`.
     * **When it's used**:
     * - In `AfSfcRequirement` when an application requires specific network functions to be applied to its traffic in a particular order.
     */
    std::optional<std::string> sfcIdDl;

    /**
     * @brief Contains **opaque information for service functions in the N6-LAN**.
     * **How it's used**:
     * - Provided by the AF and transparently sent to the UPF, to be used by Service Functions within the N6-LAN.
     * **When it's used**:
     * - In `AfSfcRequirement` or `TrafficControlData` when using Service Function Chaining to pass application-specific metadata to network functions in the chain.
     */
    std::optional<std::string> metadata;
};

/**
 * @brief Represents traffic steering parameters for multi-access PDU sessions
 */
struct TrafficSteeringParameters {
    /**
     * @brief Indicates the **Steering Functionality** for Multi-Access PDU Sessions (e.g., `MPTCP`, `MPQUIC`, `ATSSS_LL`).
     * **How it's used**:
     * - Provides details on how traffic should be transported, particularly relevant for advanced steering mechanisms.
     * **When it's used**:
     * - In `TrafficControlData` in conjunction with `steerFun` for specific traffic transport requirements.
     */
    std::optional<std::string> transMode;

    /**
     * @brief Specifies the **Steering Mode for Downlink traffic** (e.g., `ACTIVE_STANDBY`, `LOAD_BALANCING`, `SMALLEST_DELAY`, `PRIORITY_BASED`, `REDUNDANT`).
     * **How it's used**:
     * - The PCF authorizes specific steering modes for traffic distribution across different accesses.
     * - The PCF can provide a PCC Rule for non-MPTCP/non-MPQUIC traffic with `steerModeDl` set to any supported steering mode except `SMALLEST_DELAY` or `REDUNDANT`.
     * **When it's used**:
     * - In `TrafficControlData` to define how downlink traffic should be steered across multiple access types in a Multi-Access PDU Session.
     */
    std::optional<SteeringMode> steerModeDl;

    /**
     * @brief Specifies the **Steering Mode for Uplink traffic**.
     * **How it's used**:
     * - Similar to `steerModeDl`, but for uplink traffic.
     * - The PCF sets `steerModeUl` to `ACTIVE_STANDBY` for non-MPTCP/non-MPQUIC traffic.
     * **When it's used**:
     * - In `TrafficControlData` to define how uplink traffic should be steered across multiple access types in a Multi-Access PDU Session.
     */
    std::optional<SteeringMode> steerModeUl;
};

/**
 * @brief Represents traffic correlation information used in AF requests
 */
struct TrafficCorrelationParameters {
    /**
     * @brief Contains information for **traffic correlation**, identifying a set of UEs accessing an application.
     * **How it's used**:
     * - Includes Traffic Correlation ID, common EAS IP addresses, and FQDN range.
     * - The AF can influence traffic routing by providing this correlation info.
     * **When it's used**:
     * - In `AfRoutingRequirement` to indicate that traffic from multiple UEs should be treated in a coordinated way.
     */
    std::optional<TrafficCorrelationInfo> tfcCorreInfo;
};

/**
 * @brief Represents a comprehensive request for Traffic Path Management
 * (routing, steering, and service function chaining) by an Application Function (AF) to the 5G network.
 */
struct AfTrafficPathRequest {
    UeIdentifiers ueIds;
    TrafficRoutingParameters trafficRouting;
    ServiceFunctionChainParameters serviceFunctionChain;
    TrafficSteeringParameters trafficSteering;
    TrafficCorrelationParameters trafficCorrelation;
};

#endif // AF_CORE_MODEL_TRAFFIC_PATH_H