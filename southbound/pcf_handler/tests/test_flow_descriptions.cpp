/**
 * @file test_flow_descriptions.cpp
 * @brief Unit tests for flow description utility functions and port propagation.
 *
 * Tests cover:
 *  - format_flow_description()  — IPFilterRule string formatting
 *  - collect_port_specs()       — port/range to string conversion
 *  - All port combination scenarios (both, device-only, server-only, none)
 *  - Port range handling (single ports, ranges, mixed)
 *  - Protocol is always "ip" (any) per CAMARA specification
 */

#include <gtest/gtest.h>
#include "flow_description_utils.h"

using namespace af::southbound::flow_utils;
using af::common::qod::PortsSpec;
using af::common::qod::PortRange;

// ===========================================================================
// format_flow_description tests
// ===========================================================================

class FormatFlowDescriptionTest : public ::testing::Test {};

TEST_F(FormatFlowDescriptionTest, UplinkNoPorts) {
    auto result = format_flow_description("out", "192.168.1.1", "", "10.0.0.1", "");
    EXPECT_EQ(result, "permit out ip from 192.168.1.1 to 10.0.0.1");
}

TEST_F(FormatFlowDescriptionTest, DownlinkNoPorts) {
    auto result = format_flow_description("in", "10.0.0.1", "", "192.168.1.1", "");
    EXPECT_EQ(result, "permit in ip from 10.0.0.1 to 192.168.1.1");
}

TEST_F(FormatFlowDescriptionTest, UplinkWithSinglePorts) {
    auto result = format_flow_description("out", "192.168.1.1", "5000", "10.0.0.1", "8080");
    EXPECT_EQ(result, "permit out ip from 192.168.1.1 5000 to 10.0.0.1 8080");
}

TEST_F(FormatFlowDescriptionTest, DownlinkWithSinglePorts) {
    auto result = format_flow_description("in", "10.0.0.1", "8080", "192.168.1.1", "5000");
    EXPECT_EQ(result, "permit in ip from 10.0.0.1 8080 to 192.168.1.1 5000");
}

TEST_F(FormatFlowDescriptionTest, UplinkWithPortRanges) {
    auto result = format_flow_description("out", "192.168.1.1", "5000-5100", "10.0.0.1", "8080-8090");
    EXPECT_EQ(result, "permit out ip from 192.168.1.1 5000-5100 to 10.0.0.1 8080-8090");
}

TEST_F(FormatFlowDescriptionTest, OnlySourcePort) {
    auto result = format_flow_description("out", "192.168.1.1", "5000", "10.0.0.1", "");
    EXPECT_EQ(result, "permit out ip from 192.168.1.1 5000 to 10.0.0.1");
}

TEST_F(FormatFlowDescriptionTest, OnlyDestPort) {
    auto result = format_flow_description("out", "192.168.1.1", "", "10.0.0.1", "8080");
    EXPECT_EQ(result, "permit out ip from 192.168.1.1 to 10.0.0.1 8080");
}

TEST_F(FormatFlowDescriptionTest, AnyAddresses) {
    auto result = format_flow_description("out", "any", "", "any", "");
    EXPECT_EQ(result, "permit out ip from any to any");
}

TEST_F(FormatFlowDescriptionTest, ProtocolIsAlwaysIp) {
    // The protocol field must always be "ip" (any protocol) per CAMARA spec
    auto result = format_flow_description("out", "any", "", "any", "");
    EXPECT_NE(result.find(" ip "), std::string::npos);
    // Must not contain protocol numbers like "17" (UDP) or "6" (TCP)
    EXPECT_EQ(result.find(" 17 "), std::string::npos);
    EXPECT_EQ(result.find(" 6 "), std::string::npos);
}

TEST_F(FormatFlowDescriptionTest, Ipv6Addresses) {
    auto result = format_flow_description(
        "out", "2001:db8:85a3::1", "5000", "2001:db8:1::1", "8080");
    EXPECT_EQ(result,
        "permit out ip from 2001:db8:85a3::1 5000 to 2001:db8:1::1 8080");
}

// ===========================================================================
// collect_port_specs tests
// ===========================================================================

class CollectPortSpecsTest : public ::testing::Test {};

TEST_F(CollectPortSpecsTest, EmptyPortsSpec) {
    PortsSpec ports;
    auto specs = collect_port_specs(ports);
    EXPECT_TRUE(specs.empty());
}

TEST_F(CollectPortSpecsTest, SinglePort) {
    PortsSpec ports;
    ports.ports = {5000};
    auto specs = collect_port_specs(ports);
    ASSERT_EQ(specs.size(), 1u);
    EXPECT_EQ(specs[0], "5000");
}

TEST_F(CollectPortSpecsTest, MultiplePorts) {
    PortsSpec ports;
    ports.ports = {5000, 5001, 8080};
    auto specs = collect_port_specs(ports);
    ASSERT_EQ(specs.size(), 3u);
    EXPECT_EQ(specs[0], "5000");
    EXPECT_EQ(specs[1], "5001");
    EXPECT_EQ(specs[2], "8080");
}

TEST_F(CollectPortSpecsTest, SingleRange) {
    PortsSpec ports;
    ports.ranges = {{5000, 5100}};
    auto specs = collect_port_specs(ports);
    ASSERT_EQ(specs.size(), 1u);
    EXPECT_EQ(specs[0], "5000-5100");
}

TEST_F(CollectPortSpecsTest, DegenerateRangeTreatedAsSinglePort) {
    // A range where from == to should produce a single port string
    PortsSpec ports;
    ports.ranges = {{5000, 5000}};
    auto specs = collect_port_specs(ports);
    ASSERT_EQ(specs.size(), 1u);
    EXPECT_EQ(specs[0], "5000");
}

TEST_F(CollectPortSpecsTest, MultipleRanges) {
    PortsSpec ports;
    ports.ranges = {{5000, 5100}, {6000, 6200}};
    auto specs = collect_port_specs(ports);
    ASSERT_EQ(specs.size(), 2u);
    EXPECT_EQ(specs[0], "5000-5100");
    EXPECT_EQ(specs[1], "6000-6200");
}

TEST_F(CollectPortSpecsTest, MixedPortsAndRanges) {
    PortsSpec ports;
    ports.ports = {80, 443};
    ports.ranges = {{5000, 5100}};
    auto specs = collect_port_specs(ports);
    ASSERT_EQ(specs.size(), 3u);
    // Individual ports come first, then ranges
    EXPECT_EQ(specs[0], "80");
    EXPECT_EQ(specs[1], "443");
    EXPECT_EQ(specs[2], "5000-5100");
}

TEST_F(CollectPortSpecsTest, BoundaryPorts) {
    PortsSpec ports;
    ports.ports = {0, 65535};
    auto specs = collect_port_specs(ports);
    ASSERT_EQ(specs.size(), 2u);
    EXPECT_EQ(specs[0], "0");
    EXPECT_EQ(specs[1], "65535");
}

TEST_F(CollectPortSpecsTest, FullPortRange) {
    PortsSpec ports;
    ports.ranges = {{0, 65535}};
    auto specs = collect_port_specs(ports);
    ASSERT_EQ(specs.size(), 1u);
    EXPECT_EQ(specs[0], "0-65535");
}

// ===========================================================================
// Integration: flow description generation with various port configurations
// ===========================================================================

class FlowDescriptionIntegrationTest : public ::testing::Test {
protected:
    // Simulates what build_flow_descriptions / map_ports_to_media_subcomponents
    // do with various port configurations, using the shared utility functions.

    struct FlowInput {
        std::string src_ip = "192.168.1.100";
        std::string dst_ip = "10.0.0.1";
        std::optional<PortsSpec> device_ports;
        std::optional<PortsSpec> server_ports;
    };

    // Generates pairs of (uplink, downlink) flow descriptions
    std::vector<std::string> generate_flows(const FlowInput& input) {
        std::vector<std::string> flows;
        std::vector<std::string> src_specs;
        std::vector<std::string> dst_specs;

        if (input.device_ports && !input.device_ports->is_empty()) {
            src_specs = collect_port_specs(*input.device_ports);
        }
        if (input.server_ports && !input.server_ports->is_empty()) {
            dst_specs = collect_port_specs(*input.server_ports);
        }

        if (!src_specs.empty() && !dst_specs.empty()) {
            for (const auto& sp : src_specs) {
                for (const auto& dp : dst_specs) {
                    flows.push_back(format_flow_description("out", input.src_ip, sp, input.dst_ip, dp));
                    flows.push_back(format_flow_description("in", input.dst_ip, dp, input.src_ip, sp));
                }
            }
        } else if (!src_specs.empty()) {
            for (const auto& sp : src_specs) {
                flows.push_back(format_flow_description("out", input.src_ip, sp, input.dst_ip, ""));
                flows.push_back(format_flow_description("in", input.dst_ip, "", input.src_ip, sp));
            }
        } else if (!dst_specs.empty()) {
            for (const auto& dp : dst_specs) {
                flows.push_back(format_flow_description("out", input.src_ip, "", input.dst_ip, dp));
                flows.push_back(format_flow_description("in", input.dst_ip, dp, input.src_ip, ""));
            }
        } else {
            flows.push_back(format_flow_description("out", input.src_ip, "", input.dst_ip, ""));
            flows.push_back(format_flow_description("in", input.dst_ip, "", input.src_ip, ""));
        }

        return flows;
    }
};

TEST_F(FlowDescriptionIntegrationTest, NoPorts_GenericBidirectionalFlow) {
    FlowInput input;
    auto flows = generate_flows(input);
    ASSERT_EQ(flows.size(), 2u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 to 10.0.0.1");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 to 192.168.1.100");
}

TEST_F(FlowDescriptionIntegrationTest, SinglePortBothSides) {
    FlowInput input;
    PortsSpec dp, sp;
    dp.ports = {5000};
    sp.ports = {8080};
    input.device_ports = dp;
    input.server_ports = sp;

    auto flows = generate_flows(input);
    ASSERT_EQ(flows.size(), 2u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 5000 to 10.0.0.1 8080");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 8080 to 192.168.1.100 5000");
}

TEST_F(FlowDescriptionIntegrationTest, PortRangeBothSides) {
    FlowInput input;
    PortsSpec dp, sp;
    dp.ranges = {{5000, 5100}};
    sp.ranges = {{8080, 8090}};
    input.device_ports = dp;
    input.server_ports = sp;

    auto flows = generate_flows(input);
    ASSERT_EQ(flows.size(), 2u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 5000-5100 to 10.0.0.1 8080-8090");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 8080-8090 to 192.168.1.100 5000-5100");
}

TEST_F(FlowDescriptionIntegrationTest, MultiplePortsBothSides_CrossProduct) {
    FlowInput input;
    PortsSpec dp, sp;
    dp.ports = {5000, 5001};
    sp.ports = {8080, 8081};
    input.device_ports = dp;
    input.server_ports = sp;

    auto flows = generate_flows(input);
    // 2 device ports × 2 server ports × 2 directions = 8 flows
    ASSERT_EQ(flows.size(), 8u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 5000 to 10.0.0.1 8080");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 8080 to 192.168.1.100 5000");
    EXPECT_EQ(flows[2], "permit out ip from 192.168.1.100 5000 to 10.0.0.1 8081");
    EXPECT_EQ(flows[3], "permit in ip from 10.0.0.1 8081 to 192.168.1.100 5000");
    EXPECT_EQ(flows[4], "permit out ip from 192.168.1.100 5001 to 10.0.0.1 8080");
    EXPECT_EQ(flows[5], "permit in ip from 10.0.0.1 8080 to 192.168.1.100 5001");
    EXPECT_EQ(flows[6], "permit out ip from 192.168.1.100 5001 to 10.0.0.1 8081");
    EXPECT_EQ(flows[7], "permit in ip from 10.0.0.1 8081 to 192.168.1.100 5001");
}

TEST_F(FlowDescriptionIntegrationTest, MixedPortsAndRanges) {
    FlowInput input;
    PortsSpec dp, sp;
    dp.ports = {5000};
    dp.ranges = {{6000, 6100}};
    sp.ports = {8080};
    input.device_ports = dp;
    input.server_ports = sp;

    auto flows = generate_flows(input);
    // 2 device specs (1 port + 1 range) × 1 server port × 2 directions = 4 flows
    ASSERT_EQ(flows.size(), 4u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 5000 to 10.0.0.1 8080");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 8080 to 192.168.1.100 5000");
    EXPECT_EQ(flows[2], "permit out ip from 192.168.1.100 6000-6100 to 10.0.0.1 8080");
    EXPECT_EQ(flows[3], "permit in ip from 10.0.0.1 8080 to 192.168.1.100 6000-6100");
}

TEST_F(FlowDescriptionIntegrationTest, OnlyDevicePorts) {
    FlowInput input;
    PortsSpec dp;
    dp.ports = {5000};
    input.device_ports = dp;

    auto flows = generate_flows(input);
    ASSERT_EQ(flows.size(), 2u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 5000 to 10.0.0.1");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 to 192.168.1.100 5000");
}

TEST_F(FlowDescriptionIntegrationTest, OnlyServerPorts) {
    FlowInput input;
    PortsSpec sp;
    sp.ports = {8080};
    input.server_ports = sp;

    auto flows = generate_flows(input);
    ASSERT_EQ(flows.size(), 2u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 to 10.0.0.1 8080");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 8080 to 192.168.1.100");
}

TEST_F(FlowDescriptionIntegrationTest, OnlyDevicePortRange) {
    FlowInput input;
    PortsSpec dp;
    dp.ranges = {{5000, 5100}};
    input.device_ports = dp;

    auto flows = generate_flows(input);
    ASSERT_EQ(flows.size(), 2u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 5000-5100 to 10.0.0.1");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 to 192.168.1.100 5000-5100");
}

TEST_F(FlowDescriptionIntegrationTest, OnlyServerPortRange) {
    FlowInput input;
    PortsSpec sp;
    sp.ranges = {{8080, 8090}};
    input.server_ports = sp;

    auto flows = generate_flows(input);
    ASSERT_EQ(flows.size(), 2u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 to 10.0.0.1 8080-8090");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 8080-8090 to 192.168.1.100");
}

TEST_F(FlowDescriptionIntegrationTest, MultiplePortRangesBothSides) {
    FlowInput input;
    PortsSpec dp, sp;
    dp.ranges = {{5000, 5100}, {6000, 6100}};
    sp.ranges = {{8080, 8090}};
    input.device_ports = dp;
    input.server_ports = sp;

    auto flows = generate_flows(input);
    // 2 device ranges × 1 server range × 2 directions = 4 flows
    ASSERT_EQ(flows.size(), 4u);
    EXPECT_EQ(flows[0], "permit out ip from 192.168.1.100 5000-5100 to 10.0.0.1 8080-8090");
    EXPECT_EQ(flows[1], "permit in ip from 10.0.0.1 8080-8090 to 192.168.1.100 5000-5100");
    EXPECT_EQ(flows[2], "permit out ip from 192.168.1.100 6000-6100 to 10.0.0.1 8080-8090");
    EXPECT_EQ(flows[3], "permit in ip from 10.0.0.1 8080-8090 to 192.168.1.100 6000-6100");
}

// ===========================================================================
// Edge cases and regression tests
// ===========================================================================

class FlowDescriptionEdgeCaseTest : public ::testing::Test {};

TEST_F(FlowDescriptionEdgeCaseTest, EmptyPortsSpecIsNotUsed) {
    // An empty PortsSpec (no ports or ranges) should not produce port specs
    PortsSpec empty;
    EXPECT_TRUE(empty.is_empty());
    auto specs = collect_port_specs(empty);
    EXPECT_TRUE(specs.empty());
}

TEST_F(FlowDescriptionEdgeCaseTest, NoProtocolNumberInOutput) {
    // Ensure no numeric protocol identifiers leak into output
    // This is a regression test for the original "17" (UDP) bug
    auto result = format_flow_description("out", "192.168.1.1", "5000", "10.0.0.1", "8080");
    // Must contain "ip" as the protocol keyword
    EXPECT_NE(result.find("ip from"), std::string::npos);
    // Must not contain "17" anywhere
    EXPECT_EQ(result.find("17"), std::string::npos);
}

TEST_F(FlowDescriptionEdgeCaseTest, FlowDescriptionContainsToKeyword) {
    // Regression: original build_flow_filters was missing "to" when ports were present
    auto result = format_flow_description("out", "192.168.1.1", "5000", "10.0.0.1", "8080");
    EXPECT_NE(result.find(" to "), std::string::npos);
}

TEST_F(FlowDescriptionEdgeCaseTest, CidrNotationPreserved) {
    // Application server addresses can include CIDR notation
    auto result = format_flow_description("out", "192.168.1.1", "", "198.51.100.0/24", "8080");
    EXPECT_EQ(result, "permit out ip from 192.168.1.1 to 198.51.100.0/24 8080");
}
