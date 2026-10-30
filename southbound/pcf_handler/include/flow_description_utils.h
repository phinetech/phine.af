/**
 * @file flow_description_utils.h
 * @brief Utility functions for building 3GPP IPFilterRule flow descriptions.
 *
 * These utilities format flow description strings that conform to the
 * IPFilterRule syntax defined in RFC 6733 and used by TS 29.214/29.514.
 *
 * The protocol is always "ip" (any protocol) because the CAMARA QoD API
 * does not expose protocol selection to API consumers.
 *
 * Flow description format:
 *   permit {in|out} ip from <src> [<port_spec>] to <dst> [<port_spec>]
 *
 * Where:
 *   - "in"  = downlink (toward UE, from network perspective)
 *   - "out" = uplink   (from UE,   from network perspective)
 *   - <port_spec> = single port (e.g. "5000") or range (e.g. "5000-5100")
 */

#pragma once

#include <string>
#include <utility>
#include <vector>
#include <models/qod/qod_session.h>

namespace af {
namespace southbound {
namespace flow_utils {

/**
 * @brief Collect all port specification strings from a PortsSpec.
 *
 * Converts individual ports and port ranges into IPFilterRule port spec
 * strings.  Single ports become e.g. "5000", ranges become e.g. "5000-5100".
 * A degenerate range where from == to is treated as a single port.
 *
 * @param ports Port specification containing individual ports and/or ranges
 * @return Vector of formatted port spec strings
 */
inline std::vector<std::string> collect_port_specs(const af::common::qod::PortsSpec& ports) {
    std::vector<std::string> specs;
    specs.reserve(ports.ports.size() + ports.ranges.size());

    for (const auto port : ports.ports) {
        specs.push_back(std::to_string(port));
    }
    for (const auto& range : ports.ranges) {
        if (range.from == range.to) {
            specs.push_back(std::to_string(range.from));
        } else {
            specs.push_back(std::to_string(range.from) + "-" + std::to_string(range.to));
        }
    }
    return specs;
}

/**
 * @brief Build a single IPFilterRule flow description string.
 *
 * Produces a flow description conforming to the 3GPP IPFilterRule format
 * used in TS 29.214/29.514.  The protocol is always "ip" (any protocol)
 * because the CAMARA QoD API does not expose protocol selection.
 *
 * @param direction "out" (downlink, toward UE) or "in" (uplink, from UE)
 * @param src_ip    Source IP address or "any"
 * @param src_port  Source port spec string; empty means any port
 * @param dst_ip    Destination IP address or "any"
 * @param dst_port  Destination port spec string; empty means any port
 * @return Formatted IPFilterRule string
 */
inline std::string format_flow_description(const std::string& direction, const std::string& src_ip,
                                           const std::string& src_port, const std::string& dst_ip,
                                           const std::string& dst_port) {
    std::string flow = "permit " + direction + " ip from " + src_ip;
    if (!src_port.empty()) {
        flow += " " + src_port;
    }
    flow += " to " + dst_ip;
    if (!dst_port.empty()) {
        flow += " " + dst_port;
    }
    return flow;
}

/**
 * @brief Build the uplink/downlink IPFilterRule pair for one port combination.
 *
 * On N5 a flow description is encoded per TS 29.514 clause 5.6.3.2, which
 * defers to TS 29.214 clause 5.3.8: the direction "in" refers to uplink IP
 * flows and "out" to downlink IP flows, and each description is written from
 * the sender's point of view.  An uplink filter therefore runs UE -> server
 * and a downlink filter server -> UE.
 *
 * Do not reuse this encoding for N7: there (TS 29.212 clause 5.4.2) the
 * direction is always "out", the UE end is the destination, and uplink vs
 * downlink travels in a separate "flowDirection" attribute.
 *
 * Centralised here so the handler and its tests cannot drift apart; emitting
 * "out" for the uplink made a PCF see no uplink SDF, authorize an uplink MBR
 * of 0 and reject any uplink GBR the same request asked for.
 *
 * @param ue_ip       UE / device IP address or "any"
 * @param ue_port     UE port spec string; empty means any port
 * @param server_ip   Application server IP address or "any"
 * @param server_port Application server port spec string; empty means any port
 * @return {uplink description, downlink description}
 */
inline std::pair<std::string, std::string> make_flow_description_pair(const std::string& ue_ip,
                                                                      const std::string& ue_port,
                                                                      const std::string& server_ip,
                                                                      const std::string& server_port) {
    return {
        format_flow_description("in", ue_ip, ue_port, server_ip, server_port),
        format_flow_description("out", server_ip, server_port, ue_ip, ue_port),
    };
}

} // namespace flow_utils
} // namespace southbound
} // namespace af
