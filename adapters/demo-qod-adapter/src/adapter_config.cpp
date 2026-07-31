/// @file adapter_config.cpp
/// @brief Configuration loading helpers for the demo QoD adapter.

#include "adapter_config.hpp"

#include "af_config.h"
#include "af_typed_config.hpp"

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace phine::adapter {
namespace {

std::string join_key(const std::string& root_prefix, const std::string& key) {
    if (root_prefix.empty()) {
        return key;
    }

    return root_prefix + "." + key;
}

YAML::Node resolve_root_node(const YAML::Node& root, const std::string& root_prefix) {
    if (root_prefix.empty()) {
        return root;
    }

    YAML::Node current = root;
    std::size_t start = 0;
    while (start < root_prefix.size()) {
        const std::size_t dot = root_prefix.find('.', start);
        const std::string token = root_prefix.substr(start, dot - start);
        current = current[token];
        if (!current) {
            return YAML::Node{};
        }

        if (dot == std::string::npos) {
            break;
        }

        start = dot + 1;
    }

    return current;
}

std::string build_endpoint_address(
    const std::shared_ptr<af::config::Configuration>& config,
    const std::string& root_prefix,
    const std::string& endpoint_prefix,
    const std::string& fallback_value) {

    const std::string host = config->get<std::string>(
        join_key(root_prefix, endpoint_prefix + ".host"), "");
    const int port = config->get<int>(
        join_key(root_prefix, endpoint_prefix + ".port"), 0);

    if (host.empty() || port <= 0) {
        return fallback_value;
    }

    const af::config::EndpointConfig endpoint{host, static_cast<std::uint16_t>(port)};
    return endpoint.host + ":" + af::config::port_to_string(endpoint.port);
}

std::vector<StreamConfig> parse_streams(const YAML::Node& root) {
    std::vector<StreamConfig> streams;
    auto node = root["streams"];
    if (!node || !node.IsSequence()) {
        return streams;
    }

    for (const auto& s : node) {
        StreamConfig sc;
        sc.name = s["name"].as<std::string>("");
        sc.description = s["description"].as<std::string>("");
        sc.qos_profile = s["qos_profile"].as<std::string>("QOS_S");
        sc.duration_seconds = s["duration_seconds"].as<int>(3600);

        if (auto dev = s["device"]) {
            if (auto ip = dev["ipv4_address"]) {
                DeviceIpv4Addr addr;
                addr.public_address = ip["public_address"].as<std::string>("");
                addr.public_port = ip["public_port"].as<int>(0);
                sc.device.ipv4_address = addr;
            }
            if (dev["phone_number"]) {
                sc.device.phone_number = dev["phone_number"].as<std::string>();
            }
        }

        if (auto as = s["application_server"]) {
            sc.app_server.ipv4_address = as["ipv4_address"].as<std::string>("0.0.0.0/0");
        }

        if (auto dp = s["device_ports"]) {
            PortsSpec spec;
            if (dp["ports"] && dp["ports"].IsSequence()) {
                for (const auto& port : dp["ports"]) {
                    spec.ports.push_back(port.as<int>());
                }
            }
            if (dp["ranges"] && dp["ranges"].IsSequence()) {
                for (const auto& range : dp["ranges"]) {
                    PortRange pr;
                    pr.from = range["from"].as<int>(0);
                    pr.to = range["to"].as<int>(0);
                    spec.ranges.push_back(pr);
                }
            }
            sc.device_ports = spec;
        }

        streams.push_back(std::move(sc));
    }

    return streams;
}

}  // namespace

AdapterRuntimeConfig load_adapter_runtime_config(
    const std::string& path,
    const AdapterRuntimeDefaults& defaults,
    const std::string& root_prefix) {

    AdapterRuntimeConfig runtime;
    runtime.client = defaults.client;
    runtime.monitor = defaults.monitor;

    const auto config = af::config::Configuration::load(path);

    runtime.client.af_core_address = config->get<std::string>(
        join_key(root_prefix, "af_core.address"),
        build_endpoint_address(config, root_prefix, "af_core.communication.remote", runtime.client.af_core_address));

    runtime.client.transport = config->get<std::string>(
        join_key(root_prefix, "af_core.transport"),
        runtime.client.transport);

    runtime.client.timeout_seconds = config->get<int>(
        join_key(root_prefix, "af_core.timeout_seconds"),
        runtime.client.timeout_seconds);

    runtime.client.max_retries = config->get<int>(
        join_key(root_prefix, "retry.max_retries"),
        runtime.client.max_retries);

    runtime.client.initial_retry_delay_ms = config->get<int>(
        join_key(root_prefix, "retry.initial_delay_ms"),
        runtime.client.initial_retry_delay_ms);

    runtime.monitor.interval_seconds = config->get<int>(
        join_key(root_prefix, "monitor.interval_seconds"),
        runtime.monitor.interval_seconds);

    runtime.monitor.iterations = config->get<int>(
        join_key(root_prefix, "monitor.iterations"),
        runtime.monitor.iterations);

    const YAML::Node yaml_root = YAML::LoadFile(path);
    const YAML::Node adapter_root = resolve_root_node(yaml_root, root_prefix);
    if (!adapter_root) {
        throw std::runtime_error("Adapter configuration root not found: " + root_prefix);
    }

    runtime.streams = parse_streams(adapter_root);
    return runtime;
}

}  // namespace phine::adapter