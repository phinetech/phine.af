/// @file main.cpp
/// @brief Demo QoD Adapter entry point.
///
/// Loads configuration from config.yaml, creates QoD sessions for configured
/// traffic streams, monitors them, then cleans up on exit.

#include "adapter_config.hpp"
#include "http_qod_client.hpp"
#include "qod_client.hpp"
#include "session_manager.hpp"

#include <spdlog/spdlog.h>

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
    AdapterRuntimeConfig runtime_config;
    try {
        runtime_config = load_adapter_runtime_config(
            config_path,
            AdapterRuntimeDefaults{QodClientConfig{}, MonitorConfig{10, 6}});
    } catch (const std::exception& e) {
        spdlog::error("Failed to load config: {}", e.what());
        return 1;
    }

    const auto& client_config = runtime_config.client;
    const auto& streams = runtime_config.streams;
    const auto& monitor_config = runtime_config.monitor;

    if (streams.empty()) {
        spdlog::error("No streams configured — nothing to do.");
        return 1;
    }
    spdlog::info("Loaded {} stream definition(s)", streams.size());

    // ── Create QodClient (transport-switchable) ─────────────────────────────
    std::shared_ptr<IQodClient> client;
    if (client_config.transport == "http" || client_config.transport == "http2") {
        spdlog::info("Using HTTP/2 transport to af_core");
        client = std::make_shared<HttpQodClient>(client_config);
    } else {
        spdlog::info("Using gRPC transport to af_core");
        client = std::make_shared<QodClient>(client_config);
    }
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
