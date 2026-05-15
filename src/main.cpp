/**
 * @file main.cpp
 * @brief Main entry point for the bundled AF application
 *
 * This runs all three AF components (Northbound API, Core, Southbound PCF Handler)
 * in a single process using direct (in-memory) communication instead of gRPC
 * between components.
 */

#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <spdlog/spdlog.h>

#include "af_orchestrator.h"
#include "api_adapter.h"
#include "pcf_handler.h"

// Global pointers for signal handling
static af::northbound::ApiAdapter* g_api_adapter = nullptr;
static af::core::AfOrchestrator* g_orchestrator = nullptr;
static af::southbound::PcfHandler* g_pcf_handler = nullptr;
static std::atomic<bool> g_running{true};

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down..." << std::endl;
    g_running = false;

    // Stop components in reverse order of dependency
    if (g_api_adapter) {
        std::cout << "Stopping Northbound API server..." << std::endl;
        g_api_adapter->stop();
    }

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
        std::cout << " 5G Application Function (Bundled Mode)  " << std::endl;
        std::cout << "=========================================" << std::endl;

        // Parse command line arguments
        std::string config_path = parseConfigPath(argc, argv);
        std::cout << "Using configuration file: " << config_path << std::endl;

        // Register signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        // --- Initialize components bottom-up (southbound first) ---

        // 1. Southbound: PCF Handler
        std::cout << "[1/3] Initializing PCF Handler (southbound)..." << std::endl;
        af::southbound::PcfHandler pcf_handler(config_path);
        g_pcf_handler = &pcf_handler;
        pcf_handler.initialize();
        std::cout << "  PCF Handler initialized." << std::endl;

        // 2. Core: AF Orchestrator
        std::cout << "[2/3] Initializing AF Core (orchestrator)..." << std::endl;
        af::core::AfOrchestrator orchestrator(config_path);
        g_orchestrator = &orchestrator;
        orchestrator.initialize();
        std::cout << "  AF Core initialized." << std::endl;

        // 3. Northbound: API Adapter
        std::cout << "[3/3] Initializing API Adapter (northbound)..." << std::endl;
        af::northbound::ApiAdapter api_adapter(config_path);
        g_api_adapter = &api_adapter;
        api_adapter.initialize();
        std::cout << "  API Adapter initialized." << std::endl;

        // --- Start components bottom-up ---
        std::cout << "Starting all components..." << std::endl;

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

        // Give core and southbound a moment to start their gRPC servers
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Start API Adapter in a separate thread (this runs the HTTP/2 server)
        std::thread api_thread([&api_adapter]() {
            try {
                api_adapter.start();
            } catch (const std::exception& e) {
                std::cerr << "API Adapter error: " << e.what() << std::endl;
            }
        });

        std::cout << "=========================================" << std::endl;
        std::cout << " All components started successfully.     " << std::endl;
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
        if (api_thread.joinable()) api_thread.join();
        if (core_thread.joinable()) core_thread.join();
        if (pcf_thread.joinable()) pcf_thread.join();
        if (cleanup_thread.joinable()) cleanup_thread.join();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
