/**
 * @file main.cpp
 * @brief Main entry point for the AF Core service
 */

#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include "af_orchestrator.h"

// Global pointer to orchestrator for signal handling
af::core::AfOrchestrator* g_orchestrator = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    
    if (g_orchestrator) {
        std::cout << "Stopping AF Core services..." << std::endl;
        g_orchestrator->stop();
    }
    
    exit(signum);
}

// Parse command line arguments
std::string parseConfigPath(int argc, char* argv[]) {
    std::string config_path = "/etc/oai/af/af_core.yaml"; // Default path
    
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
        // console->info("Starting 5G AF Core Service");
        
        // Parse command line arguments
        std::string config_path = parseConfigPath(argc, argv);
        // console->info("Using configuration file: {}", config_path);
        
        // Register signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Create and initialize orchestrator
        af::core::AfOrchestrator orchestrator(config_path);
        g_orchestrator = &orchestrator;
        
        // Initialize components
        orchestrator.initialize();
        
        // Start the service
        // console->info("Starting AF Core services...");
        orchestrator.start();
        
        // Run cleanup task periodically
        // console->info("Starting subscription cleanup task...");
        
        bool running = true;
        std::thread cleanup_thread([&orchestrator, &running]() {
            while (running) {
                // Sleep for a while
                std::this_thread::sleep_for(std::chrono::minutes(10));
                
                // Clean up expired subscriptions
                auto subscription_manager = orchestrator.get_subscription_manager();
                if (subscription_manager) {
                    int removed = subscription_manager->cleanup_expired_subscriptions();
                    if (removed > 0) {
                        // console->info("Cleaned up {} expired subscriptions", removed);
                        std::cout << "Cleaned up " << removed << " expired subscriptions" << std::endl;
                    }
                }
            }
        });
        
        // Wait for cleanup thread to finish
        cleanup_thread.join();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}