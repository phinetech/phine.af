-- ============================================================================
-- OAI CN5G UDR - Policy Data Tables
-- ============================================================================
--
-- Standard: 3GPP TS 29.519 V18.8.0 (2024-09) - Nudr_PolicyData Service API
-- Related:  3GPP TS 29.519 defines the User Data Repository (UDR) Policy Data
--           service to support Policy Control Function (PCF) access to policy
--           data stored in UDR.
--
-- PURPOSE:
-- This script creates database tables to support the Nudr_PolicyData service
-- API, enabling the Policy Control Function (PCF) to retrieve policy-related
-- data for UEs during registration, PDU session establishment, and policy
-- updates.
--
-- ARCHITECTURE NOTES:
-- - Tables are structured to match 3GPP data models defined in TS 29.519
-- - JSON columns store complex nested structures (NSSAI, AMBR, QoS profiles)
-- - VARCHAR/INT columns for simple types that may be used in WHERE clauses
-- - Primary keys enable efficient lookup by UE ID (SUPI/GPSI)
-- - Composite indexes on SessionManagementPolicyData for DNN/SNSSAI queries
--
-- ============================================================================

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

-- ============================================================================
-- Table: AccessAndMobilityPolicyData
-- ============================================================================
--
-- STANDARD REFERENCE: 3GPP TS 29.519 Clause 5.2.3.2, Table 5.4.2.2-1
--                     Data Type: AmPolicyData
-- YAML DEFINITION:    TS29519_Policy_Data.yaml
-- API ENDPOINT:       GET /policy-data/ues/{ueId}/am-data
-- QUERIED BY:         PCF during UE registration (via AMF)
--
-- PURPOSE:
-- Stores Access and Mobility (AM) policy data for individual UEs as defined
-- in the AmPolicyData type. This data is used by PCF to make policy decisions
-- for access and mobility management, including presence reporting areas,
-- charging function selection, spending limit control, and restricted status.
--
-- COLUMN DERIVATION:
-- All columns are derived directly from the AmPolicyData data type definition
-- in 3GPP TS 29.519 V18.8.0 Table 5.4.2.2-1:
--   - praInfos: map(PresenceInfo) - Presence reporting area information
--   - subscCats: array(string) - Subscription categories
--   - chfInfo: ChargingInformation - Charging Function information
--   - subscSpendingLimits: boolean - Spending limit control indicator
--   - spendLimInfo: map(PolicyCounterInfo) - Policy counter status
--   - restriStatus: array(RestrictedStatus) - Restricted status list
--   - suppFeat: SupportedFeatures - Supported features (conditional)
--
-- STRUCTURE RATIONALE:
-- - ueid (PRIMARY KEY): UE identifier for fast lookup (not in standard type,
--   but required as the resource identifier from the URI path)
-- - All standard attributes stored as JSON to preserve exact 3GPP data structures
-- - No denormalization into scalar columns as all fields are complex types or
--   may contain multiple values (maps/arrays)
--
-- USAGE FLOW:
-- 1. PCF queries: GET /nudr-dr/v2/policy-data/ues/{ueId}/am-data
-- 2. UDR HTTP/2 server extracts ueId from URL path
-- 3. udr_app calls mysql_db::query_am_policy_data(ue_id, json_response)
-- 4. MySQL executes: SELECT * FROM AccessAndMobilityPolicyData WHERE ueid='...'
-- 5. Result row is converted to JSON matching AmPolicyData type
-- 6. PCF uses data for AM policy decisions
--
-- ============================================================================

CREATE TABLE IF NOT EXISTS `AccessAndMobilityPolicyData` (
  `ueid` varchar(20) NOT NULL COMMENT 'UE identifier (SUPI/GPSI) - URI path parameter, not in AmPolicyData type',
  `praInfos` json DEFAULT NULL COMMENT 'map(PresenceInfo): Presence reporting area information. TS29519 Table 5.4.2.2-1. Key is praId.',
  `subscCats` json DEFAULT NULL COMMENT 'array(string): List of subscription categories. TS29519 Table 5.4.2.2-1.',
  `chfInfo` json DEFAULT NULL COMMENT 'ChargingInformation: CHF addresses, instance ID, set ID. TS29519 Table 5.4.2.2-1. Feature: CHFInformation.',
  `subscSpendingLimits` tinyint(1) DEFAULT NULL COMMENT 'boolean: Whether PCF enforces spending limits. TS29519 Table 5.4.2.2-1. Feature: SLAMUP.',
  `spendLimInfo` json DEFAULT NULL COMMENT 'map(PolicyCounterInfo): Policy counter status. TS29519 Table 5.4.2.2-1. Key is policyCounterId. Feature: SLAMUP.',
  `restriStatus` json DEFAULT NULL COMMENT 'array(RestrictedStatus): Restricted status with reason and timestamp. TS29519 Table 5.4.2.2-1. Feature: AbnormalBehaviour.',
  `suppFeat` varchar(50) DEFAULT NULL COMMENT 'SupportedFeatures: Negotiated features from NF consumer request. TS29519 Table 5.4.2.2-1.',
  PRIMARY KEY (`ueid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='AM Policy Data per TS 29.519 Table 5.4.2.2-1 (AmPolicyData type)';

-- ============================================================================
-- Table: SessionManagementPolicyData
-- ============================================================================
--
-- STANDARD REFERENCE: 3GPP TS 29.519 Table 5.4.2.5-1 - SmPolicyData
-- TEXT SPEC:          ts_129519v180800p.txt
-- YAML DEFINITION:    TS29519_Policy_Data.yaml
-- API ENDPOINT:       GET /policy-data/ues/{ueId}/sm-data?dnn={dnn}&snssai={snssai}
-- QUERIED BY:         PCF during PDU session establishment (via SMF)
--
-- PURPOSE:
-- Stores Session Management (SM) policy data for UEs as defined in the
-- SmPolicyData type. This data structure contains nested maps of policy
-- information organized by S-NSSAI and DNN, supporting PCF policy decisions
-- for PDU session establishment and management.
--
-- COLUMN DERIVATION:
-- All columns derived directly from SmPolicyData type (TS 29.519 Table 5.4.2.5-1):
--   - smPolicySnssaiData: map(SmPolicySnssaiData) (M, 1..N) - Policy data per S-NSSAI
--     Each SmPolicySnssaiData contains:
--       * snssai: Snssai (M) - The S-NSSAI identifier
--       * smPolicyDnnData: map(SmPolicyDnnData) (O) - Policy data per DNN within the slice
--       * ueSliceMbr: SliceMbr (O) - Slice-level maximum bit rate
--   - umDataLimits: map(UsageMonDataLimit) (O, 1..N) - Usage monitoring data limits
--   - umData: map(UsageMonData) (O, 1..N) - Usage monitoring data
--   - suppFeat: SupportedFeatures (C, 0..1) - Supported features
--
-- STRUCTURE RATIONALE:
-- - Single JSON column per UE storing complete SmPolicyData structure
-- - Preserves exact 3GPP nested map hierarchy: SmPolicyData → map[S-NSSAI] → SmPolicySnssaiData → map[DNN] → SmPolicyDnnData
-- - Query parameters (dnn, snssai) are applied in C++ code after retrieval
-- - Simpler schema, no denormalization needed
-- - Standard-compliant: API response = direct JSON serialization of database field
--
-- QUERY PATTERN:
-- 1. UDR retrieves: SELECT smPolicySnssaiData, umDataLimits, umData, suppFeat FROM SessionManagementPolicyData WHERE ueid=?
-- 2. Construct SmPolicyData object from 4 columns
-- 3. Apply optional filters in C++ code:
--    - If snssai parameter present: filter smPolicySnssaiData map by key
--    - If dnn parameter present: filter smPolicyDnnData map by key within each SmPolicySnssaiData
-- 4. Return filtered SmPolicyData as JSON response
--
-- EXAMPLE JSON STRUCTURE:
-- {
--   "smPolicySnssaiData": {
--     "{\"sst\":222,\"sd\":\"00007B\"}": {
--       "snssai": {"sst": 222, "sd": "00007B"},
--       "smPolicyDnnData": {
--         "default": {
--           "dnn": "default",
--           "subscCats": ["premium"],
--           "gbrUl": "100Mbps",
--           ...
--         }
--       }
--     }
--   },
--   "suppFeat": "3fff"
-- }
--
-- ============================================================================

CREATE TABLE IF NOT EXISTS `SessionManagementPolicyData` (
  `ueid` varchar(20) NOT NULL COMMENT 'UE identifier (SUPI/GPSI) - URI path parameter',
  `smPolicySnssaiData` json NOT NULL COMMENT 'map(SmPolicySnssaiData): Policy data per S-NSSAI. TS29519 Table 5.4.2.5-1. Mandatory. Key is S-NSSAI string.',
  `umDataLimits` json DEFAULT NULL COMMENT 'map(UsageMonDataLimit): Usage monitoring data limits. TS29519 Table 5.4.2.5-1. Optional. Key is limitId.',
  `umData` json DEFAULT NULL COMMENT 'map(UsageMonData): Usage monitoring data. TS29519 Table 5.4.2.5-1. Optional. Key is limitId.',
  `suppFeat` varchar(50) DEFAULT NULL COMMENT 'SupportedFeatures: Negotiated features from NF consumer request. TS29519 Table 5.4.2.5-1. Conditional.',
  PRIMARY KEY (`ueid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='SM Policy Data per TS 29.519 Table 5.4.2.5-1 (SmPolicyData type)';

-- --------------------------------------------------------

-- ============================================================================
-- SAMPLE DATA
-- ============================================================================
--
-- The following INSERT statements provide sample policy data for testing and
-- development. These entries match UE identifiers (SUPIs) from the existing
-- AuthenticationSubscription and AccessAndMobilitySubscriptionData tables in
-- oai_db_v2.sql.
--
-- UE ID RANGES:
-- - 208950000000031-035: Test UEs with slice sst=222, sd=00007B
-- - 208950000000125-129: Test UEs with slice sst=1, sd=000001
--
-- PRODUCTION DEPLOYMENT:
-- Replace these samples with actual subscriber policy data from your
-- operator's provisioning system. Policy data should align with:
-- - Subscription tier (premium/standard/basic)
-- - Enterprise vs consumer subscribers
-- - Geographic/regulatory restrictions
-- - Service agreements (SLA, QoS commitments)
--
-- --------------------------------------------------------

--
-- Sample data for `AccessAndMobilityPolicyData`
--
-- NOTES:
-- - praInfos: Presence reporting areas for location tracking (NULL = not configured)
-- - subscCats: Subscription categories for policy selection (NULL = no special categories)
-- - chfInfo: Charging function addresses (NULL = use default CHF from PCF config)
-- - subscSpendingLimits: Spending limit control disabled (0) for test users
-- - spendLimInfo: Policy counter status (NULL = no active counters)
-- - restriStatus: Restricted access status (NULL = no restrictions)
-- - suppFeat: Supported features negotiated with PCF (NULL = no feature negotiation)
--
-- All sample entries use NULL for policy attributes, meaning default PCF behavior
-- applies. In production, populate these fields based on subscriber policy profile.
--
INSERT INTO `AccessAndMobilityPolicyData` (`ueid`, `praInfos`, `subscCats`, `chfInfo`, `subscSpendingLimits`, `spendLimInfo`, `restriStatus`, `suppFeat`) VALUES
('208950000000031', '{}', '["standard"]', '{}', 0, '{}', '[]', ''),
('208950000000032', '{}', '["standard"]', '{}', 0, '{}', '[]', ''),
('208950000000033', '{}', '["standard"]', '{}', 0, '{}', '[]', ''),
('208950000000034', '{}', '["standard"]', '{}', 0, '{}', '[]', ''),
('208950000000035', '{}', '["standard"]', '{}', 0, '{}', '[]', ''),
('208950000000125', '{}', '["premium"]', '{}', 0, '{}', '[]', ''),
('208950000000126', '{}', '["premium"]', '{}', 0, '{}', '[]', ''),
('208950000000127', '{}', '["premium"]', '{}', 0, '{}', '[]', ''),
('208950000000128', '{}', '["premium"]', '{}', 0, '{}', '[]', ''),
('208950000000129', '{}', '["premium"]', '{}', 0, '{}', '[]', '');

-- --------------------------------------------------------

-- ============================================================================
-- Table: UePolicySet
-- ============================================================================
--
-- STANDARD REFERENCE: 3GPP TS 29.519 Clause 5.2.6.3.5 - UePolicySet
-- API ENDPOINT: GET /policy-data/ues/{ueId}/ue-policy-set
-- QUERIED BY: PCF for UE route selection policy (URSP), application-specific policies
--
-- PURPOSE:
-- Stores UE-level policy sets that define application routing rules, presence
-- reporting area configurations, and UE-specific policy sections. This enables
-- advanced features like:
-- - URSP (UE Route Selection Policy): Which PDU session to use per application
-- - ANDSP (Access Network Discovery and Selection Policy): Which access network to prefer
-- - Presence Reporting Areas: Geographic regions where UE location is reported
-- - Application-specific policies: QoS/routing rules per OS, device type, or app
--
-- STRUCTURE RATIONALE:
-- - Composite PRIMARY KEY (ueid, policySetId): Allows multiple policy sets per UE
--   (e.g., "default", "enterprise", "roaming"). Most UEs use "default".
-- - JSON fields store complex policy structures defined in TS 29.512/29.514
-- - policySetId defaults to "default" for standard single-policy-set deployments
-- - praInfos (Presence Reporting Area): JSON array of geographic area definitions
-- - uePolicySections: Application-specific routing/QoS rules (matches TS 29.512)
--
-- USAGE FLOW:
-- 1. PCF queries: GET /nudr-dr/v2/policy-data/ues/{ueId}/ue-policy-set
-- 2. UDR executes: SELECT * FROM UePolicySet WHERE ueid='...'
-- 3. Returns policy set(s) with URSP rules, allowed route descriptors, etc.
-- 4. PCF provisions URSP to UE via NAS signaling (through AMF)
-- 5. UE uses URSP to select which PDU session/DNN to use for each application
--
-- EXAMPLE USE CASE:
-- A UE with two PDU sessions (DNN="internet" and DNN="enterprise"):
-- - URSP rule 1: Work email app → enterprise DNN (higher security)
-- - URSP rule 2: Web browser → internet DNN (normal routing)
-- - URSP rule 3: Video streaming → internet DNN with specific QoS
--
-- KEY FIELDS:
-- - uePolicySections: Array of policy rules with traffic descriptors and DNN selection
-- - allowedRouteSelDescriptors: Permitted routing options (URSP enforcement)
-- - praInfos: List of geographic areas for location-based policy triggers
-- - andspInd: Boolean indicating ANDSP policy is active (access network selection)
-- - osIds: Operating system identifiers for OS-specific policies (iOS, Android, etc.)
--
-- NOTE:
-- This table is less frequently used than AM/SM policy data. Many deployments
-- use static URSP provisioned in AMF configuration rather than per-UE policies.
-- Include data here only if dynamic, UE-specific routing policies are required.
-- ============================================================================

CREATE TABLE IF NOT EXISTS `UePolicySet` (
  `ueid` varchar(20) NOT NULL COMMENT 'UE identifier (SUPI/GPSI)',
  `policySetId` varchar(50) NOT NULL DEFAULT 'default' COMMENT 'UE Policy Set identifier',
  `praInfos` json DEFAULT NULL COMMENT 'map(PresenceInfo): Presence Reporting Area information. Key is praId.',
  `subscCats` json DEFAULT NULL COMMENT 'array(string): Subscription categories.',
  `uePolicySections` json DEFAULT NULL COMMENT 'map(UePolicySection): UE policy sections with traffic descriptors. Key is policySetId.',
  `upsis` json DEFAULT NULL COMMENT 'array(string): UE Policy Section Identifiers.',
  `allowedRouteSelDescs` json DEFAULT NULL COMMENT 'map(PlmnRouteSelectionDescriptor): Allowed route selection descriptors. Key is plmnId.',
  `andspInd` tinyint(1) DEFAULT NULL COMMENT 'boolean: ANDSP (Access Network Discovery and Selection Policy) indicator.',
  `pei` varchar(50) DEFAULT NULL COMMENT 'string: Permanent Equipment Identifier (IMEI).',
  `osIds` json DEFAULT NULL COMMENT 'array(string): Operating System identifiers for OS-specific policies.',
  `suppFeat` varchar(50) DEFAULT NULL COMMENT 'SupportedFeatures: Negotiated features from NF consumer request.',
  PRIMARY KEY (`ueid`,`policySetId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='UE Policy Set per TS 29.519';

-- --------------------------------------------------------

--
-- Sample data for `SessionManagementPolicyData`
--
-- NOTES:
-- - smPolicyData: Complete SmPolicyData JSON structure per TS 29.519 Table 5.4.2.5-1
-- - smPolicySnssaiData: Map with S-NSSAI as key, SmPolicySnssaiData as value
-- - Each SmPolicySnssaiData contains:
--   * snssai: The slice identifier {sst, sd}
--   * smPolicyDnnData: Map with DNN as key, SmPolicyDnnData as value
--     - SmPolicyDnnData contains all per-DNN policy attributes (27 fields from Table 5.4.2.15-1)
-- - Sample entries use minimal policy data (most optional fields omitted)
-- - In production, populate based on subscriber policy profile and SLA
--
-- STRUCTURE:
-- The nested map hierarchy matches the 3GPP standard:
--   SmPolicyData
--   ├── smPolicySnssaiData (map, key = S-NSSAI string)
--   │   └── SmPolicySnssaiData
--   │       ├── snssai: {sst, sd}
--   │       └── smPolicyDnnData (map, key = DNN string)
--   │           └── SmPolicyDnnData: {dnn, subscCats, gbrUl, gbrDl, ...}
--   ├── umDataLimits (map, optional)
--   ├── umData (map, optional)
--   └── suppFeat (string, conditional)
--

INSERT INTO `SessionManagementPolicyData` (`ueid`, `smPolicySnssaiData`, `umDataLimits`, `umData`, `suppFeat`) VALUES
('208950000000031', '{"{\\"sst\\":222,\\"sd\\":\\"00007b\\"}":{"snssai":{"sst":222,"sd":"00007b"},"smPolicyDnnData":{"default":{"dnn":"default","subscCats":["standard"],"gbrUl":"50 Mbps","gbrDl":"100 Mbps","5qi":9,"arp":{"priorityLevel":8,"preemptCap":"NOT_PREEMPT","preemptVuln":"NOT_PREEMPTABLE"}}},"ueSliceMbr":{"uplink":"1Gbps","downlink":"2Gbps"}}}', '{}', '{}', ''),
('208950000000032', '{"{\\"sst\\":222,\\"sd\\":\\"00007b\\"}":{"snssai":{"sst":222,"sd":"00007b"},"smPolicyDnnData":{"default":{"dnn":"default","subscCats":["standard"],"gbrUl":"50 Mbps","gbrDl":"100 Mbps","5qi":9,"arp":{"priorityLevel":8,"preemptCap":"NOT_PREEMPT","preemptVuln":"NOT_PREEMPTABLE"}}},"ueSliceMbr":{"uplink":"1Gbps","downlink":"2Gbps"}}}', '{}', '{}', ''),
('208950000000033', '{"{\\"sst\\":222,\\"sd\\":\\"00007b\\"}":{"snssai":{"sst":222,"sd":"00007b"},"smPolicyDnnData":{"default":{"dnn":"default","subscCats":["standard"],"gbrUl":"50 Mbps","gbrDl":"100 Mbps","5qi":9,"arp":{"priorityLevel":8,"preemptCap":"NOT_PREEMPT","preemptVuln":"NOT_PREEMPTABLE"}}},"ueSliceMbr":{"uplink":"1Gbps","downlink":"2Gbps"}}}', '{}', '{}', ''),
('208950000000034', '{"{\\"sst\\":222,\\"sd\\":\\"00007b\\"}":{"snssai":{"sst":222,"sd":"00007b"},"smPolicyDnnData":{"default":{"dnn":"default","subscCats":["standard"],"gbrUl":"50 Mbps","gbrDl":"100 Mbps","5qi":9,"arp":{"priorityLevel":8,"preemptCap":"NOT_PREEMPT","preemptVuln":"NOT_PREEMPTABLE"}}},"ueSliceMbr":{"uplink":"1Gbps","downlink":"2Gbps"}}}', '{}', '{}', ''),
('208950000000035', '{"{\\"sst\\":222,\\"sd\\":\\"00007b\\"}":{"snssai":{"sst":222,"sd":"00007b"},"smPolicyDnnData":{"default":{"dnn":"default","subscCats":["standard"],"gbrUl":"50 Mbps","gbrDl":"100 Mbps","5qi":9,"arp":{"priorityLevel":8,"preemptCap":"NOT_PREEMPT","preemptVuln":"NOT_PREEMPTABLE"}}},"ueSliceMbr":{"uplink":"1Gbps","downlink":"2Gbps"}}}', '{}', '{}', '');
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
