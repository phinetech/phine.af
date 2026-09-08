/**
 * @file main.cpp
 * @brief Main entry point for the bundled AF application
 *
 * This runs the AF Core and the enabled southbound handlers in a single process.
 * Northbound applications remain standalone processes and connect to AF Core
 * over gRPC.
 */

#include <iostream>
#include <string>
#include <csignal>
#include <spdlog/spdlog.h>

#include "bundled_af_runtime.h"

static af::app::BundledAfRuntime* g_runtime = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down..." << std::endl;
    if (g_runtime) {
        g_runtime->requestShutdown();
    }
}

// Parse command line arguments
std::string parseConfigPath(int argc, char* argv[]) {
    std::string config_path = "/etc/phine.af/af.yaml"; // Default path

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc) {
                config_path = argv[i + 1];
                i++;
            }
        }
    }

    return config_path;
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "=========================================" << std::endl;
        std::cout << " 5G Application Function (Core/SB Bundle) " << std::endl;
        std::cout << "=========================================" << std::endl;

        // Parse command line arguments
        std::string config_path = parseConfigPath(argc, argv);
        std::cout << "Using configuration file: " << config_path << std::endl;

        // Register signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        af::app::BundledAfRuntime runtime(config_path);
        g_runtime = &runtime;
        runtime.initialize();
        runtime.start();

        std::cout << "=========================================" << std::endl;
        std::cout << " Bundled AF started successfully.         " << std::endl;
        std::cout << " AF Core is available for northbound apps." << std::endl;
        std::cout << " Press Ctrl+C to stop.                    " << std::endl;
        std::cout << "=========================================" << std::endl;

        runtime.wait();
        g_runtime = nullptr;

        return 0;
    } catch (const std::exception& e) {
        if (g_runtime) {
            g_runtime->requestShutdown();
            g_runtime = nullptr;
        }
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
