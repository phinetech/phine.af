/**
 * @file main.cpp
 * @brief Main entry point for the PCF Handler service
 */

#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include "pcf_handler.h"

// Global pointer to handler for signal handling
af::southbound::PcfHandler* g_pcf_handler = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    
    if (g_pcf_handler) {
        std::cout << "Stopping PCF Handler service..." << std::endl;
        g_pcf_handler->stop();
    }
    
    exit(signum);
}

// Parse command line arguments
std::string parseConfigPath(int argc, char* argv[]) {
    std::string config_path = "/etc/oai/af/southbound/pcf_handler.yaml"; // Default path
    
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
        // console->info("Starting 5G AF PCF Handler Service");
        
        // Parse command line arguments
        std::string config_path = parseConfigPath(argc, argv);
        // console->info("Using configuration file: {}", config_path);
        
        // Register signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Create and initialize PCF handler
        af::southbound::PcfHandler pcf_handler(config_path);
        g_pcf_handler = &pcf_handler;
        
        // Initialize components
        pcf_handler.initialize();
        
        // Start the service
        // console->info("Starting PCF Handler service...");
        pcf_handler.start();
        
        // Main loop - keep the service running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}