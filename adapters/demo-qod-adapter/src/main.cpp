/// @file main.cpp
/// @brief Demo QoD Adapter entry point.
///
/// Loads configuration from config.yaml, creates QoD sessions for configured
/// traffic streams, monitors them, then cleans up on exit.

#include "qod_client.hpp"
#include "session_manager.hpp"

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

// ─── Graceful shutdown ──────────────────────────────────────────────────────

static std::atomic<bool> g_shutdown_requested{false};

static void signal_handler(int /*signum*/) {
    g_shutdown_requested.store(true, std::memory_order_release);
}

// ─── Configuration parsing ──────────────────────────────────────────────────

namespace {

using namespace phine::adapter;

QodClientConfig parse_client_config(const YAML::Node& root) {
    QodClientConfig cfg;
    if (auto af = root["af_core"]) {
        if (af["address"])         cfg.af_core_address      = af["address"].as<std::string>();
        if (af["timeout_seconds"]) cfg.timeout_seconds       = af["timeout_seconds"].as<int>();
    }
    if (auto retry = root["retry"]) {
        if (retry["max_retries"])       cfg.max_retries            = retry["max_retries"].as<int>();
        if (retry["initial_delay_ms"])  cfg.initial_retry_delay_ms = retry["initial_delay_ms"].as<int>();
    }
    return cfg;
}

std::vector<StreamConfig> parse_streams(const YAML::Node& root) {
    std::vector<StreamConfig> streams;
    auto node = root["streams"];
    if (!node || !node.IsSequence()) return streams;

    for (const auto& s : node) {
        StreamConfig sc;
        sc.name             = s["name"].as<std::string>("");
        sc.description      = s["description"].as<std::string>("");
        sc.qos_profile      = s["qos_profile"].as<std::string>("QOS_S");
        sc.duration_seconds = s["duration_seconds"].as<int>(3600);

        // Device
        if (auto dev = s["device"]) {
            if (auto ip = dev["ipv4_address"]) {
                DeviceIpv4Addr addr;
                addr.public_address = ip["public_address"].as<std::string>("");
                addr.public_port    = ip["public_port"].as<int>(0);
                sc.device.ipv4_address = addr;
            }
            if (dev["phone_number"]) {
                sc.device.phone_number = dev["phone_number"].as<std::string>();
            }
        }

        // Application server
        if (auto as = s["application_server"]) {
            sc.app_server.ipv4_address = as["ipv4_address"].as<std::string>("0.0.0.0/0");
        }

        // Device ports
        if (auto dp = s["device_ports"]) {
            PortsSpec ps;
            if (dp["ports"] && dp["ports"].IsSequence()) {
                for (const auto& p : dp["ports"]) {
                    ps.ports.push_back(p.as<int>());
                }
            }
            if (dp["ranges"] && dp["ranges"].IsSequence()) {
                for (const auto& r : dp["ranges"]) {
                    PortRange pr;
                    pr.from = r["from"].as<int>(0);
                    pr.to   = r["to"].as<int>(0);
                    ps.ranges.push_back(pr);
                }
            }
            sc.device_ports = ps;
        }

        streams.push_back(std::move(sc));
    }
    return streams;
}

struct MonitorConfig {
    int interval_seconds = 10;
    int iterations       = 6;
};

MonitorConfig parse_monitor_config(const YAML::Node& root) {
    MonitorConfig mc;
    if (auto m = root["monitor"]) {
        if (m["interval_seconds"]) mc.interval_seconds = m["interval_seconds"].as<int>();
        if (m["iterations"])       mc.iterations       = m["iterations"].as<int>();
    }
    return mc;
}

}  // namespace

// ─── Main ───────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    using namespace phine::adapter;

    // Set up logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

    // Register signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Determine config path
    std::string config_path = "config.yaml";
    if (argc > 1) {
        config_path = argv[1];
    }

    spdlog::info("Demo QoD Adapter starting — config: {}", config_path);

    // ── Load configuration ──────────────────────────────────────────────────
    YAML::Node root;
    try {
        root = YAML::LoadFile(config_path);
    } catch (const YAML::Exception& e) {
        spdlog::error("Failed to load config: {}", e.what());
        return 1;
    }

    auto client_config  = parse_client_config(root);
    auto streams        = parse_streams(root);
    auto monitor_config = parse_monitor_config(root);

    if (streams.empty()) {
        spdlog::error("No streams configured — nothing to do.");
        return 1;
    }
    spdlog::info("Loaded {} stream definition(s)", streams.size());

    // ── Create QodClient ────────────────────────────────────────────────────
    auto client = std::make_shared<QodClient>(client_config);
    if (!client->wait_for_ready(client_config.timeout_seconds)) {
        spdlog::error("af_core not reachable — aborting.");
        return 1;
    }

    // ── Create SessionManager ───────────────────────────────────────────────
    SessionManager manager(client);

    // ── Create sessions for all streams ─────────────────────────────────────
    manager.create_all_sessions(streams);

    // ── Monitor loop ────────────────────────────────────────────────────────
    // Run for N iterations, or indefinitely if iterations < 0
    for (int i = 0;
         (monitor_config.iterations < 0 || i < monitor_config.iterations) &&
         !g_shutdown_requested.load(std::memory_order_acquire);
         ++i) {
        std::this_thread::sleep_for(
            std::chrono::seconds(monitor_config.interval_seconds));

        if (g_shutdown_requested.load(std::memory_order_acquire)) break;
        manager.monitor_sessions();
    }

    // ── Cleanup ─────────────────────────────────────────────────────────────
    spdlog::info("Shutting down — cleaning up sessions…");
    manager.cleanup_all_sessions();

    // ── Summary ─────────────────────────────────────────────────────────────
    spdlog::info("Demo QoD Adapter finished.");
    return 0;
}
