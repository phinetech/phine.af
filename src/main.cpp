/**
 * @file main.cpp
 * @brief Main entry point for the bundled AF application
 *
 * This runs the AF Core and southbound PCF Handler in a single process.
 * Northbound applications remain standalone processes and connect to AF Core
 * over gRPC.
 */

#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <spdlog/spdlog.h>

#include "af_orchestrator.h"
#include "pcf_handler.h"

// Global pointers for signal handling
static af::core::AfOrchestrator* g_orchestrator = nullptr;
static af::southbound::PcfHandler* g_pcf_handler = nullptr;
static std::atomic<bool> g_running{true};

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down..." << std::endl;
    g_running = false;

    // Stop components in reverse order of dependency
    if (g_orchestrator) {
        std::cout << "Stopping AF Core..." << std::endl;
        g_orchestrator->stop();
    }

    if (g_pcf_handler) {
        std::cout << "Stopping PCF Handler..." << std::endl;
        g_pcf_handler->stop();
    }

    exit(signum);
}

// Parse command line arguments
std::string parseConfigPath(int argc, char* argv[]) {
    std::string config_path = "/etc/oai/af/af.yaml"; // Default path

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

        // --- Initialize bundled components bottom-up (southbound first) ---

        // 1. Southbound: PCF Handler
        std::cout << "[1/2] Initializing PCF Handler (southbound)..." << std::endl;
        af::southbound::PcfHandler pcf_handler(config_path);
        g_pcf_handler = &pcf_handler;
        pcf_handler.initialize();
        std::cout << "  PCF Handler initialized." << std::endl;

        // 2. Core: AF Orchestrator
        std::cout << "[2/2] Initializing AF Core (orchestrator)..." << std::endl;
        af::core::AfOrchestrator orchestrator(config_path);
        g_orchestrator = &orchestrator;
        orchestrator.initialize();
        std::cout << "  AF Core initialized." << std::endl;

        // --- Start bundled components ---
        std::cout << "Starting bundled AF Core and southbound components..." << std::endl;

        // Start PCF Handler in a separate thread
        std::thread pcf_thread([&pcf_handler]() {
            try {
                pcf_handler.start();
            } catch (const std::exception& e) {
                std::cerr << "PCF Handler error: " << e.what() << std::endl;
            }
        });

        // Start AF Core in a separate thread
        std::thread core_thread([&orchestrator]() {
            try {
                orchestrator.start();
            } catch (const std::exception& e) {
                std::cerr << "AF Core error: " << e.what() << std::endl;
            }
        });

        // Give core and southbound a moment to start their communication services
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "=========================================" << std::endl;
        std::cout << " Bundled AF started successfully.         " << std::endl;
        std::cout << " AF Core is available for northbound apps." << std::endl;
        std::cout << " Press Ctrl+C to stop.                    " << std::endl;
        std::cout << "=========================================" << std::endl;

        // Periodic cleanup task (from af_core)
        std::thread cleanup_thread([&orchestrator]() {
            while (g_running) {
                std::this_thread::sleep_for(std::chrono::minutes(10));
                if (!g_running) break;

                auto subscription_manager = orchestrator.get_subscription_manager();
                if (subscription_manager) {
                    int removed = subscription_manager->cleanup_expired_subscriptions();
                    if (removed > 0) {
                        std::cout << "Cleaned up " << removed << " expired subscriptions" << std::endl;
                    }
                }
            }
        });

        // Wait for threads
        if (core_thread.joinable()) core_thread.join();
        if (pcf_thread.joinable()) pcf_thread.join();
        if (cleanup_thread.joinable()) cleanup_thread.join();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
