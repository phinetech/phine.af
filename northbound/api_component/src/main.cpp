// main.cpp
#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include "api_adapter.h"

// Global pointer to API adapter for signal handling
af::northbound::ApiAdapter* g_api_adapter = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;

    if (g_api_adapter) {
        std::cout << "Stopping API server..." << std::endl;
        g_api_adapter->stop();
    }

    exit(signum);
}

// Parse command line arguments
std::string parseConfigPath(int argc, char* argv[]) {
    std::string config_path = "/etc/phine.af/api_adapter.yaml"; // Default path

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc) {
                config_path = argv[i + 1];
                i++; // Skip the next argument (the path)
            }
        }
    }

    return config_path;
}

int main(int argc, char* argv[]) {
    try {
        // Configure global logger
        // spdlog::set_level(spdlog::level::info);
        // auto console = spdlog::get("console"); // spdlog::stdout_color_mt("console");
        // console->info("Starting 5G AF HTTP/2 API Server");
        // Use inbuilt-in printer for console output
        std::cout << "Starting 5G AF HTTP/2 API Server" << std::endl;

        // // Parse command line arguments
        std::string config_path = parseConfigPath(argc, argv);
        // console->info("Using configuration file: {}", config_path);

        // Register signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        // Create and initialize API adapter
        af::northbound::ApiAdapter api_adapter(config_path);
        g_api_adapter = &api_adapter;

        try {
            // Initialize the API adapter
            api_adapter.initialize();
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize logger: " << e.what() << std::endl;
            return 1;
        }

        // // Start the API server (this will block)
        // console->info("Starting HTTP/2 API server...");
        try {
            api_adapter.start();
        } catch (const std::exception& e) {
            std::cerr << "Failed to start API server: " << e.what() << std::endl;
            return 1;
        }
        // console->info("5G AF HTTP/2 API Server is running... Press Ctrl+C to stop.\n");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}