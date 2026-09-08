/// @file bundled_entry.cpp
/// @brief Demo QoD Adapter bundled runtime entry.

#include "adapter_config.hpp"
#include "http_qod_client.hpp"
#include "qod_client.hpp"
#include "session_manager.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace phine::adapter;

constexpr const char* kBundleConfigEnv = "PHINE_DEMO_QOD_ADAPTER_CONFIG";
constexpr const char* kDefaultBundleConfigPath = "/etc/phine.af/demo_qod_adapter.yaml";
constexpr const char* kFallbackAfConfigPath = "/etc/phine.af/af.yaml";

bool file_exists(const std::string& path) {
    std::ifstream stream(path);
    return stream.good();
}
std::pair<std::string, std::string> load_bundle_config_location() {
    if (const char* env_path = std::getenv(kBundleConfigEnv)) {
        return {env_path, {}};
    }

    if (file_exists(kDefaultBundleConfigPath)) {
        return {kDefaultBundleConfigPath, {}};
    }

    if (file_exists(kFallbackAfConfigPath)) {
        return {kFallbackAfConfigPath, "demo_qod_adapter"};
    }

    return {{}, {}};
}

class BundledAdapterRuntime {
public:
    BundledAdapterRuntime() {
        worker_ = std::thread(&BundledAdapterRuntime::run, this);
    }

    ~BundledAdapterRuntime() {
        shutdown_requested_.store(true, std::memory_order_release);
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    BundledAdapterRuntime(const BundledAdapterRuntime&) = delete;
    BundledAdapterRuntime& operator=(const BundledAdapterRuntime&) = delete;

private:
    void run() {
        try {
            auto [config_path, root_prefix] = load_bundle_config_location();
            if (config_path.empty()) {
                spdlog::warn(
                    "[demo-qod-adapter] No bundled adapter config found. "
                    "Set {} or provide {}.",
                    kBundleConfigEnv, kDefaultBundleConfigPath);
                return;
            }

            spdlog::info("[demo-qod-adapter] Starting bundled adapter runtime using config {}", config_path);

            const auto runtime_config = load_adapter_runtime_config(
                config_path, AdapterRuntimeDefaults{QodClientConfig{}, MonitorConfig{10, -1}}, root_prefix);

            const auto& client_config = runtime_config.client;
            const auto& streams = runtime_config.streams;
            const auto& monitor_config = runtime_config.monitor;

            if (streams.empty()) {
                spdlog::warn("[demo-qod-adapter] No streams configured. Bundled adapter will remain idle.");
                return;
            }

            std::shared_ptr<IQodClient> client;
            if (client_config.transport == "http" || client_config.transport == "http2") {
                spdlog::info("[demo-qod-adapter] Using HTTP/2 transport to af_core");
                client = std::make_shared<HttpQodClient>(client_config);
            } else {
                spdlog::info("[demo-qod-adapter] Using gRPC transport to af_core");
                client = std::make_shared<QodClient>(client_config);
            }
            if (!client->wait_for_ready(client_config.timeout_seconds)) {
                spdlog::error("[demo-qod-adapter] AF Core is not reachable from bundled adapter runtime.");
                return;
            }

            SessionManager manager(client);
            manager.create_all_sessions(streams);

            for (int iteration = 0; !shutdown_requested_.load(std::memory_order_acquire) &&
                                    (monitor_config.iterations < 0 || iteration < monitor_config.iterations);
                 ++iteration) {
                std::this_thread::sleep_for(std::chrono::seconds(monitor_config.interval_seconds));

                if (shutdown_requested_.load(std::memory_order_acquire)) {
                    break;
                }

                manager.monitor_sessions();
            }

            spdlog::info("[demo-qod-adapter] Bundled adapter shutting down. Cleaning up sessions.");
            manager.cleanup_all_sessions();
        } catch (const std::exception& ex) {
            spdlog::error("[demo-qod-adapter] Bundled runtime failed: {}", ex.what());
        }
    }

    std::atomic<bool> shutdown_requested_{false};
    std::thread worker_;
};

BundledAdapterRuntime g_bundled_adapter_runtime;

} // namespace