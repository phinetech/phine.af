#ifndef AF_CORE_MODEL_TSN_H
#define AF_CORE_MODEL_TSN_H

#include <string>
#include <vector>
#include <optional>
#include "common.h"


/**
 * @brief Contains Time Sensitive Networking (TSN) QoS parameters
 */
struct TsnQosParameters {
    /**
     * @brief Contains **Time Sensitive Networking (TSN) Traffic QoS** parameters.
     * **How it's used**:
     * - Includes `maxTscBurstSize` (maximum TSC burst size), `tscPackDelay` (packet delay budget for TSC), 
     *   `maxPer` (maximum packet error rate for TSC), and `tscPrioLevel` (TSC Priority Level).
     * - The `TSN AF` or `TSCTSF` provides this information.
     * **When it's used**:
     * - When the `TimeSensitiveNetworking` feature is supported, for `Npcf_PolicyAuthorization_Create` or 
     *   `Npcf_PolicyAuthorization_Update` to describe `TSC traffic QoS characteristics`.
     */
    struct TsnQosContainer {
        std::optional<BitRate> maxTscBurstSize;
        std::optional<PacketDelBudget> tscPackDelay;
        std::optional<PacketErrRate> maxPer;
        std::optional<TscPriorityLevel> tscPrioLevel;
    };
    std::optional<TsnQosContainer> tsnQos;
    
    /**
     * @brief Packet Delay Budget (`PDB`) for the service data flow.
     * **How it's used**:
     * - A QoS characteristic specified by the AF for a service data flow.
     * - Included in `TsnQosContainer` as `tscPackDelay`.
     * **When it's used**:
     * - In AF requests (`Npcf_PolicyAuthorization_Create`/`Update`) to indicate the maximum delay tolerated for packets of a given QoS flow.
     * - For `PIN scenarios` (Personal IoT Networks) to specify Packet Delay Budget.
     */
    std::optional<PacketDelBudget> pdb;

    /**
     * @brief Packet Error Rate (`PER`) for the service data flow.
     * **How it's used**:
     * - A QoS characteristic.
     * - The AF can provide `PER` as input to policy authorization.
     * - Included in `TsnQosContainer` as `maxPer`.
     * **When it's used**:
     * - In AF requests (`Npcf_PolicyAuthorization_Create`/`Update`) to indicate the maximum packet error rate tolerated for a QoS flow.
     * - AF can request `PER` for `QoS and Alt-QoS`.
     */
    std::optional<PacketErrRate> per;
};

/**
 * @brief Contains Time Sensitive Communication Assistance Information (TSCAI) parameters
 */
struct TscaiParameters {
    /**
     * @brief Contains **TSCAI (Time Sensitive Communication Assistance Information) input for Uplink**.
     * **How it's used**:
     * - Describes the `TSC traffic pattern` including `periodicity`, `burstArrivalTime`, `surTimeInNumMsg` (survival time in number of messages), 
     *   and `surTimeInTime` (survival time in time).
     * - Used by the `TSN AF` or `TSCTSF` to provide traffic pattern details to the PCF.
     * - The SMF derives `TSCAI` and forwards it to `NG-RAN`.
     * **When it's used**:
     * - During `Npcf_PolicyAuthorization_Create` or `Npcf_PolicyAuthorization_Update` to inform the network about the expected uplink TSC traffic characteristics.
     */
    std::optional<TscaiInputContainer> tscaiInputUl;

    /**
     * @brief Contains **TSCAI (Time Sensitive Communication Assistance Information) input for Downlink**.
     * **How it's used**:
     * - Similar to `tscaiInputUl`, but for downlink traffic patterns.
     * **When it's used**:
     * - During `Npcf_PolicyAuthorization_Create` or `Npcf_PolicyAuthorization_Update` to inform the network about the expected downlink TSC traffic characteristics.
     */
    std::optional<TscaiInputContainer> tscaiInputDl;

    /**
     * @brief Indicates the **Time Domain** for Time Sensitive Communication (`TSC`).
     * **How it's used**:
     * - Specifies the time domain reference for TSC assistance.
     * **When it's used**:
     * - In AF requests for `TSC-related policy authorization`.
     */
    std::optional<Uinteger> tscaiTimeDom;
    
    /**
     * @brief Indicates the periodicity of **Uplink traffic** (in milliseconds).
     * **How it's used**:
     * - Used by the SMF to derive `Time Sensitive Communication Assistance Information (TSCAI)` and forward it to NG-RAN.
     * **When it's used**:
     * - In AF requests (`Npcf_PolicyAuthorization_Create`/`Update`) as part of `TrafficParaData` to define `TSC traffic patterns`.
     * - For `power saving` via `periodicity measurement and reporting`.
     */
    std::optional<DurationMilliSec> periodicityUl;

    /**
     * @brief Indicates the periodicity of **Downlink traffic** (in milliseconds).
     * **How it's used**:
     * - Used by the SMF to derive `TSCAI` and forward it to NG-RAN.
     * **When it's used**:
     * - In AF requests (`Npcf_PolicyAuthorization_Create`/`Update`) as part of `TrafficParaData` to define `TSC traffic patterns`.
     * - For `power saving` via `periodicity measurement and reporting`.
     */
    std::optional<DurationMilliSec> periodicityDl;
};

/**
 * @brief Contains Time Synchronization parameters
 */
struct TimeSynchronizationParameters {
    /**
     * @brief AS (Application Server) Time Distribution parameters.
     * **How it's used**:
     * - Contains information related to time distribution services for applications.
     * - Used for time synchronization services over 5G.
     * **When it's used**:
     * - When an application requires precise time synchronization services.
     */
    std::optional<AsTimeDistributionParam> asTimeDistParam;
    
    /**
     * @brief Indicates the **capability for the AF to adjust the burst sending time**.
     * **How it's used**:
     * - If `true`, signifies that the AF can adapt its traffic patterns based on network conditions to optimize burst transmission.
     * **When it's used**:
     * - In AF requests to inform the PCF about the application's adaptability to network conditions, especially relevant for `Time Sensitive Communication` (TSC).
     */
    std::optional<bool> capBatAdaptation;
    
    /**
     * @brief Indicates that the **service data flow needs to meet the Round-Trip (RT) latency requirement**.
     * **How it's used**:
     * - If set to `true`, it signifies the critical RT latency needs of the application.
     * **When it's used**:
     * - In AF requests (`Npcf_PolicyAuthorization_Create`/`Update`) to indicate delay-critical services.
     */
    std::optional<bool> rTLatencyInd;

    /**
     * @brief Correlation identifier for **Round-Trip (RT) latency monitoring** over two QoS flows.
     * **How it's used**:
     * - Allows correlation of `RTT` monitoring results for traffic traversing different QoS flows. This supports `UL` and `DL` policy control based on `Round-Trip latency requirements`.
     * **When it's used**:
     * - In AF requests to specify that RTT monitoring should correlate two distinct QoS flows.
     */
    std::optional<RttFlowReference> rTLatencyIndCorreId;
};

/**
 * @brief Represents Time Sensitive Networking (TSN) related requests from an Application Function
 */
struct AfTsnRequest {
    // Include all the component structs
    UeIdentifiers ueIds;
    TsnQosParameters tsnQosParams;
    TscaiParameters tscaiParams;
    TimeSynchronizationParameters timeSyncParams;
    TrafficFilteringInformation trafficFiltering;
    
    /**
     * @brief Indicates the **status of service data flows** (e.g., `ENABLED`, `DISABLED`, `REMOVED`).
     * **How it's used**:
     * - Indicates whether the TSN service data flow(s) should be enabled or disabled.
     * **When it's used**:
     * - During policy authorization to control the activation or deactivation of TSN traffic flows.
     */
    std::optional<std::string> flowStatus;
    
    /**
     * @brief Reference to a **pre-defined QoS information set** for TSN.
     * **How it's used**:
     * - The AF can provide this reference instead of individual QoS parameters when requesting TSN QoS.
     * **When it's used**:
     * - When the AF wants to use a standardized or pre-configured TSN QoS profile.
     */
    std::optional<std::string> qosReference;
};

#endif // AF_CORE_MODEL_TSN_H