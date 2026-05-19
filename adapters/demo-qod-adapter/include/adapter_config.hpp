#pragma once
/// @file adapter_config.hpp
/// @brief Shared configuration loading helpers for the demo QoD adapter.

#include "qod_client.hpp"
#include "session_manager.hpp"

#include <string>
#include <vector>

namespace phine::adapter {

struct MonitorConfig {
    int interval_seconds = 10;
    int iterations = 6;
};

struct AdapterRuntimeConfig {
    QodClientConfig client;
    MonitorConfig monitor;
    std::vector<StreamConfig> streams;
};

struct AdapterRuntimeDefaults {
    QodClientConfig client{};
    MonitorConfig monitor{};
};

AdapterRuntimeConfig load_adapter_runtime_config(
    const std::string& path,
    const AdapterRuntimeDefaults& defaults = {},
    const std::string& root_prefix = {});

}  // namespace phine::adapter