#ifndef AF_CORE_MODEL_QOS_H
#define AF_CORE_MODEL_QOS_H

#include <string>
#include <vector>
#include <optional>
#include "../../common/models/common.h"


// Forward declarations for nested structs to avoid circular dependencies
// These are not defined in the provided snippet, assuming their existence for context.
struct AlternativeServiceRequirementsData {};
struct TscaiInputContainer {};
struct PduSetQosParameters {};
struct QosMonitoringInfo {}; // Placeholder for QoS monitoring related control information
struct AsTimeDistributionParam {};
struct SliceUsageControlInfo {};
struct TemporalValidity {};
struct TemporalInValidity {};
struct UpPathChgEventSubscription {};
struct SteeringMode {};
struct ThresholdValue {};
struct QosRequirements {}; // For 'qosReqs'
struct EthFlowDescription {}; // For 'ethTrafficFilters'
struct FlowInfo {}; // For 'trafficFilters' / 'ipTrafficFilters'
struct ApplicationCharginId {}; // For 'afChargId'
struct ApplicationId {}; // For 'dataCollAppId'
struct TrafficCorrelationInfo {}; // For 'tfcCorreInfo'
struct EasIpReplacementInfo {}; // For 'easIpReplaceInfos'
struct NetworkAreaInfo {}; // For 'nwAreaInfo'
struct TimeWindow {}; // For 'desTimeInts'
struct DurationSec {}; // For durations in seconds
struct BitRate {}; // For various bit rates
struct PacketDelBudget {}; // For Packet Delay Budget
struct PacketErrRate {}; // For Packet Error Rate
struct Uinteger {}; // For unsigned integers
struct TscPriorityLevel {}; // For TSC Priority Level
struct RttFlowReference {}; // For RTT flow correlation
struct ProtocolDescription {}; // For protocol descriptions
struct DurationMilliSec {}; // For durations in milliseconds
struct UplinkDownlinkSupport {}; // For L4S indication
struct QosMonitoringParamType {}; // For QoS monitoring parameter type
struct RequestedQosMonitoringParameter {}; // For requested QoS monitoring parameters
struct ReportingFrequency {}; // For reporting frequency
struct GroupId {}; // For interGroupId
struct FqdnPatternMatchingRule {}; // For FQDN range
struct PdtqPolicy {}; // For PDTQ policy
struct PdtqReferenceId {}; // For PDTQ reference ID
struct QosParameterSet {}; // For QOS parameter set in PDTQ
struct SupportedFeatures {}; // For supported features
struct MbsSessionId {}; // For MBS Session ID

/**
 * @brief Represents general QoS parameters for AF sessions
 */
struct GeneralQosParameters {
    /**
     * @brief References a **pre-defined QoS information set**.
     * **How it's used**:
     * - The AF can provide a `qosReference` instead of individual QoS parameters when requesting a specific QoS for a data session.
     * - The **PCF resolves this reference to apply pre-configured QoS characteristics**.
     * **When it's used**:
     * - During `Npcf_PolicyAuthorization_Create` or `Npcf_PolicyAuthorization_Update` when the AF wants to use a standardized or pre-configured QoS profile.
     */
    std::optional<std::string> qosReference;

    /**
     * @brief Contains **individual QoS parameters requested by the AF**.
     * **How it's used**:
     * - The AF can explicitly provide detailed QoS parameters (e.g., **5QI, ARP, GBR, MBR**) associated with a flow description.
     * - The **PCF derives PCC rule QoS parameters** from these provided flow descriptions and individual QoS parameters.
     * **When it's used**:
     * - During `Npcf_PolicyAuthorization_Create` or `Npcf_PolicyAuthorization_Update` when the AF requires specific, granular QoS characteristics not covered by a `qosReference`.
     */
    std::optional<QosRequirements> qosReqs;

    /**
     * @brief Contains references to **alternative pre-defined QoS information sets**.
     * **How it's used**:
     * - The AF can provide a list of `QoS references` for alternative QoS requirements in a prioritized order.
     * - The **PCF may authorize one or more alternative parameter sets** based on these references.
     * - If the PCF receives an indication that **alternative QoS profiles are not supported by the NG-RAN**, it may include `altSerReqNotSuppInd` in notifications.
     * **When it's used**:
     * - During `Npcf_PolicyAuthorization_Create` or `Npcf_PolicyAuthorization_Update` to provide fallback QoS options if the primary requested QoS cannot be guaranteed.
     */
    std::optional<std::vector<std::string>> altSerReqs;

    /**
     * @brief Contains **alternative service requirements as individual QoS parameter sets**.
     * **How it's used**:
     * - The AF can provide a list of `Alternative Service Requirements Data`, including individual QoS parameter sets, in a prioritized order.
     * - The **PCF may derive alternative QoS parameter sets** for concerned PCC rules based on this information.
     * **When it's used**:
     * - Similar to `altSerReqs`, used when the AF provides explicit alternative QoS parameter values rather than just references.
     */
    std::optional<std::vector<AlternativeServiceRequirementsData>> altSerReqsData;

    /**
     * @brief AF application identifier (`afAppId`).
     * **How it's used**:
     * - A **key identifier that maps to a specific application traffic detection rule in the UPF**.
     * - The **PCF uses `afAppId` to determine the 5QI** (5G QoS Identifier) and **Guaranteed Data Rate** based on application-specific algorithms.
     * - It can be provided at different levels (e.g., `AppSessionContextReqData` or `MediaComponent`).
     * - The `PccRule` data type includes `appId`.
     * - The `Packet Flow Description Function (PFDF)` can consume `Application Detection analytics` from `NWDAF` using the `AppId`.
     * **When it's used**:
     * - In `Npcf_PolicyAuthorization` service operations (`Create`, `Update`) to identify the application traffic for policy control.
     * - For `MBS Session Policy Control Data`.
     * - In `PdtqData` (Planned Data Transfer with QoS).
     */
    std::optional<std::string> afAppId;

    /**
     * @brief AF charging identifier (`afChargId`).
     * **How it's used**:
     * - This information may be used for **charging correlation with QoS flow**.
     * **When it's used**:
     * - Provided by the AF to the PCF to enable correlation between application sessions and charging records.
     */
    std::optional<ApplicationCharginId> afChargId;

    /**
     * @brief Indicates the **status of service data flows** (e.g., `ENABLED`, `DISABLED`, `REMOVED`).
     * **How it's used**:
     * - The `fStatus` attribute within a media component in `medComponents` indicates whether the IP or Ethernet service data flow(s) should be enabled or disabled.
     * - If `fStatus` indicates "REMOVED", the `Guaranteed Data Rate` for Uplink and Downlink is set to 0.
     * **When it's used**:
     * - During `Npcf_PolicyAuthorization_Create` or `Npcf_PolicyAuthorization_Update` to control the activation or deactivation of specific traffic flows.
     * - As part of the `PccRule` data type (as `flowStatus`).
     */
    std::optional<std::string> flowStatus;

    /**
     * @brief Indicates whether **QoS flow parameters signalling to the UE should be disabled**.
     * **How it's used**:
     * - If set to `true`, it instructs the network to not signal QoS flow parameters to the UE.
     * - If omitted or `false`, QoS flow parameters signalling is not disabled.
     * - The SMF may omit `altQosParamId` in notifications if `disUeNotif` is not `true` and NG-RAN cannot fulfill the lowest priority alternative QoS profile.
     * **When it's used**:
     * - In AF requests to control UE notification behavior related to QoS.
     */
    std::optional<bool> disUeNotif;
};

/**
 * @brief Represents performance measurement and monitoring parameters used in AF requests
 */
struct PerformanceMeasurementMonitoring {
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

    /**
     * @brief Packet Delay Variation (`PDV`) reporting information.
     * **How it's used**:
     * - Contains `ulPdv`, `dlPdv`, `rtPdv` (uplink, downlink, round trip packet delay variation).
     * - The AF can request monitoring of `Packet Delay Variation`.
     * **When it's used**:
     * - In `PdvMonitoringReport` when reporting measured PDV values.
     * - PCF supports `5GS Packet Delay Variation value monitoring` and exposure to AF.
     */
    struct PacketDelayVariation {
        std::optional<int> ulPdv; /**< @brief Uplink packet delay variation in milliseconds. */
        std::optional<int> dlPdv; /**< @brief Downlink packet delay variation in milliseconds. */
        std::optional<int> rtPdv; /**< @brief Round trip packet delay variation in milliseconds. */
    };
    std::optional<PacketDelayVariation> packetDelayVariation;

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

    /**
     * @brief Specifies the **QoS monitoring parameter type** (e.g., `PACKET_DELAY`, `CONGESTION`, `DATA_RATE`).
     * **How it's used**:
     * - The `AF` indicates the type of QoS parameter it wishes to monitor.
     * - If the AF request includes QoS parameter measurements derived by the PCF, and direct notification is not possible, the PCF informs the AF which `qosMonParamType` is not notified directly.
     * **When it's used**:
     * - In `QosMonitoringData` to specify the kind of QoS monitoring desired.
     */
    std::optional<QosMonitoringParamType> qosMonParamType;

    /**
     * @brief Indicates the **QoS information to be monitored**.
     * **How it's used**:
     * - The `AF` can explicitly request specific QoS monitoring measurements to be performed by the network.
     * - The PCF subscribes to QoS Monitoring information for corresponding PCC rules.
     * **When it's used**:
     * - As a mandatory attribute within `QosMonitoringData` when the AF requests QoS monitoring.
     */
    std::optional<std::vector<RequestedQosMonitoringParameter>> reqQosMonParams;

    /**
     * @brief Indicates the **frequency for reporting QoS monitoring** (e.g., `EVENT_TRIGGERED`, `PERIODIC`).
     * **How it's used**:
     * - The AF specifies how often or under what conditions it wishes to receive QoS monitoring reports.
     * **When it's used**:
     * - As a mandatory attribute within `QosMonitoringData` when the AF requests QoS monitoring.
     */
    std::optional<std::vector<ReportingFrequency>> repFreqs;

    /**
     * @brief Indicates the period of time (in milliseconds) for **DL packet delay reporting threshold**.
     * **How it's used**:
     * - The AF can define thresholds for packet delay, triggering reports when exceeded.
     * **When it's used**:
     * - In `QosMonitoringData` when setting up packet delay monitoring.
     */
    std::optional<int> repThreshDl;

    /**
     * @brief Indicates the period of time (in milliseconds) for **UL packet delay reporting threshold**.
     * **How it's used**:
     * - Similar to `repThreshDl`, for uplink traffic.
     * **When it's used**:
     * - In `QosMonitoringData` when setting up packet delay monitoring.
     */
    std::optional<int> repThreshUl;

    /**
     * @brief Indicates the period of time (in milliseconds) for **round trip packet delay reporting threshold**.
     * **How it's used**:
     * - Similar to `repThreshDl`, for round trip traffic.
     * **When it's used**:
     * - In `QosMonitoringData` when setting up packet delay monitoring.
     */
    std::optional<int> repThreshRp;

    /**
     * @brief Indicates the **DL congestion information threshold** (percentage from 0 to 10000).
     * **How it's used**:
     * - The AF can define thresholds for downlink congestion, triggering reports when exceeded.
     * **When it's used**:
     * - In `QosMonitoringData` when setting up congestion monitoring.
     */
    std::optional<Uinteger> conThreshDl;

    /**
     * @brief Indicates the **UL congestion information threshold** (percentage from 0 to 10000).
     * **How it's used**:
     * - Similar to `conThreshDl`, for uplink congestion.
     * **When it's used**:
     * - In `QosMonitoringData` when setting up congestion monitoring.
     */
    std::optional<Uinteger> conThreshUl;

    /**
     * @brief Specifies the **averaging window** for QoS monitoring, typically for data rate.
     * **How it's used**:
     * - If `EnQoSMon` feature is supported and `qosMonParamType` indicates `data rate`, the `avrgWndw` can be included in `QosMonitoringData`.
     * - The PCF includes this parameter when QoS monitoring is enabled for non-GBR 5QI types.
     * **When it's used**:
     * - In `QosMonitoringData` to specify how data rates or other metrics should be averaged for reporting.
     */
    std::optional<DurationSec> avrgWndw;

    /**
     * @brief Indicates the **UL data rate reporting threshold** (in bits per second).
     * **How it's used**:
     * - The AF can define thresholds for uplink data rate, triggering reports when exceeded.
     * **When it's used**:
     * - In `QosMonitoringData` when setting up data rate monitoring.
     */
    std::optional<BitRate> repThreshDatRateUl;

    /**
     * @brief Indicates the **DL data rate reporting threshold** (in bits per second).
     * **How it's used**:
     * - Similar to `repThreshDatRateUl`, for downlink data rate.
     * **When it's used**:
     * - In `QosMonitoringData` when setting up data rate monitoring.
     */
    std::optional<BitRate> repThreshDatRateDl;

    /**
     * @brief Identifies an **Application ID** for data collection related to QoS monitoring.
     * **How it's used**:
     * - When the SMF receives this ID within `QosMonitoringData`, it associates the PCC rule with the QoS monitoring event exposure subscription for that application.
     * **When it's used**:
     * - In `QosMonitoringData` to link QoS monitoring reports to a specific application for data collection and analytics.
     */
    std::optional<ApplicationId> dataCollAppId;

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

    /**
     * @brief Indicates the **capability for the AF to adjust the burst sending time**.
     * **How it's used**:
     * - If `true`, signifies that the AF can adapt its traffic patterns based on network conditions to optimize burst transmission.
     * **When it's used**:
     * - In AF requests to inform the PCF about the application's adaptability to network conditions, especially relevant for `Time Sensitive Communication` (TSC).
     */
    std::optional<bool> capBatAdaptation;
};

/**
 * @brief Represents charging and billing parameters used in AF requests
 */
struct ChargingBillingParameters {
    /**
     * @brief Application Service Provider identifier used for sponsored data flows.
     * **How it's used**:
     * - Relevant for sponsored data flows for accounting purposes.
     * **When it's used**:
     * - When provisioning Planned Data Transfer with QoS requirements.
     */
    std::optional<std::string> aspId;

    /**
     * @brief Contains the **Planned Data Transfer with QoS (PDTQ) Policy**.
     * **How it's used**:
     * - Defines the policy for data transfers with specific QoS requirements, often for background data.
     * - The AF influences policies for `negotiation of PDTQ`.
     * - `NWDAF analytics` can be used in `PDTQ policy`.
     * **When it's used**:
     * - When the AF wants to initiate or modify a planned data transfer with specific QoS guarantees.
     */
    std::optional<PdtqPolicy> pdtqPolicy;

    /**
     * @brief Reference ID for **Planned Data Transfer with QoS (PDTQ)**.
     * **How it's used**:
     * - Provides a unique reference for a specific PDTQ instance.
     * **When it's used**:
     * - To refer to a specific PDTQ policy.
     */
    std::optional<PdtqReferenceId> pdtqRefId;

    /**
     * @brief Indicates whether **PDTQ warning notification is enabled**.
     * **How it's used**:
     * - If `true`, enables warning notifications related to the PDTQ policy.
     * **When it's used**:
     * - When configuring PDTQ to receive alerts about potential policy re-negotiation or performance degradation.
     */
    std::optional<bool> warnNotifEnabled;

    /**
     * @brief Contains the **preferred time slots** for a planned data transfer.
     * **How it's used**:
     * - The AF can provide preferred time windows for data transfer.
     * **When it's used**:
     * - In `PdtqData` to define the preferred time slots for the data transfer.
     */
    std::optional<std::vector<TimeWindow>> desTimeInts;
};

/**
 * @brief Represents protocol description parameters used in AF requests
 */
struct ProtocolDescriptionParameters {
    /**
     * @brief Protocol description of the **Downlink media flow**.
     * **How it's used**:
     * - Provides detailed protocol information for traffic detection and handling.
     * **When it's used**:
     * - In `MediaComponent` or `PccRule` to describe the protocol of the data flow, for instance, for `PDU Set Handling`.
     */
    std::optional<ProtocolDescription> protoDescDl;

    /**
     * @brief Protocol description of the **Uplink media flow**.
     * **How it's used**:
     * - Similar to `protoDescDl`, but for uplink traffic.
     * **When it's used**:
     * - In `MediaComponentsInd` to instruct the UPF on L4S marking.
     */
    std::optional<UplinkDownlinkSupport> l4sInd;

    /**
     * @brief Contains **PDU Set QoS Parameters for Downlink direction**.
     * **How it's used**:
     * - Used to support `PDU Set based QoS handling` in the downlink direction.
     * **When it's used**:
     * - In `QosData` or `MediaComponent` for applications that handle data in sets.
     */
    std::optional<PduSetQosParameters> pduSetQosDl;

    /**
     * @brief Contains **PDU Set QoS Parameters for Uplink direction**.
     * **How it's used**:
     * - Used to support `PDU Set based QoS handling` in the uplink direction.
     * **When it's used**:
     * - In `QosData` or `MediaComponent` for applications that handle data in sets.
     */
    std::optional<PduSetQosParameters> pduSetQosUl;
};

/**
 * @brief Represents the comprehensive set of QoS-related attributes that an Application Function (AF) can manage
 * by sending requests to the 5G network, typically via the Policy Control Function (PCF) or Network Exposure Function (NEF).
 */
struct AfQosRequest {
    // Include all the component structs
    UeIdentifiers ueIds;
    TrafficFilteringInformation trafficFiltering;
    GeneralQosParameters generalQosParams;
    PerformanceMeasurementMonitoring perfMeasurement;
    ChargingBillingParameters chargingBilling;
    NetworkIdentifiers networkIds;
    ProtocolDescriptionParameters protocolDesc;
};

#endif // AF_CORE_MODEL_QOS_H