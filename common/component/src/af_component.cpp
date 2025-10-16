// af_component.cpp
#include "af_component.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace af::common {

AfComponent::AfComponent(const std::string& name)
    : name_(name), running_(false), status_("INITIALIZED") {
    // Initialize the logger with the component name
    initializeLogger(spdlog::level::debug);
    
    logger_->info("Component '{}' created", name_);
}

AfComponent::~AfComponent() {
    // If the component is still running, stop it
    // Note: We can't call the pure virtual stop() method from the destructor
    // as the derived class has already been destroyed. Derived classes should
    // ensure they call stop() in their own destructors if needed.
    if (running_) {
        logger_->warn("Component '{}' destroyed while still running - derived class should call stop() in destructor", name_);
        running_ = false; // Force set to false to indicate stopped state
    }
    
    logger_->info("Component '{}' destroyed", name_);
}

bool AfComponent::isRunning() const {
    return running_;
}

void AfComponent::stop() {
    logger_->info("Stopping component '{}'", name_);
    
    // Set running state to false
    setRunning(false);
    
    // Perform any additional cleanup if necessary
    // ...
    
    logger_->info("Component '{}' stopped", name_);
}

std::string AfComponent::getName() const {
    return name_;
}

std::string AfComponent::getStatus() const {
    return status_;
}

void AfComponent::configure(const std::map<std::string, std::string>& config) {
    logger_->info("Configuring component '{}'", name_);
    
    // Base configuration handling
    for (const auto& [key, value] : config) {
        logger_->debug("Config: {} = {}", key, value);
        
        // Handle common configuration parameters here
        if (key == "log_level") {
            if (value == "trace") {
                logger_->set_level(spdlog::level::trace);
            } else if (value == "debug") {
                logger_->set_level(spdlog::level::debug);
            } else if (value == "info") {
                logger_->set_level(spdlog::level::info);
            } else if (value == "warn") {
                logger_->set_level(spdlog::level::warn);
            } else if (value == "error") {
                logger_->set_level(spdlog::level::err);
            } else if (value == "critical") {
                logger_->set_level(spdlog::level::critical);
            }
        }
    }
}

std::shared_ptr<spdlog::logger> AfComponent::getLogger() const {
    return logger_;
}

void AfComponent::setRunning(bool running) {
    running_ = running;
    
    if (running) {
        setStatus("RUNNING");
    } else {
        setStatus("STOPPED");
    }
}

void AfComponent::setStatus(const std::string& status) {
    status_ = status;
    logger_->debug("Component '{}' status changed to '{}'", name_, status_);
}

void AfComponent::initializeLogger(spdlog::level::level_enum log_level) {
    // Check if a logger with this name already exists
    logger_ = spdlog::get(name_);
    
    if (!logger_) {
        // Create a new logger with a colored console sink
        logger_ = spdlog::stdout_color_mt(name_);
    }
    
    // Set the log level
    logger_->set_level(log_level);
    
    // Set the log pattern: timestamp [level] [component] message
    logger_->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%n] %v");
}

} // namespace af::common