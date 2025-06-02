// af_component.h
#pragma once

#include <string>
#include <memory>
#include <map>
#include <spdlog/spdlog.h>

namespace af::common {

/**
 * @brief Base class for all AF components
 * 
 * This class provides a common interface for all components in the AF architecture,
 * including lifecycle management, configuration, and logging.
 */
class AfComponent {
public:
    /**
     * @brief Constructor
     * @param name The name of the component
     */
    explicit AfComponent(const std::string& name);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~AfComponent();
    
    /**
     * @brief Initialize the component
     * 
     * This method should be overridden by derived classes to perform component initialization.
     * It will be called once before the component is started.
     */
    virtual void initialize() = 0;
    
    /**
     * @brief Start the component
     * 
     * This method should be overridden by derived classes to start the component's operation.
     * It will be called after the component is initialized.
     */
    virtual void start() = 0;
    
    /**
     * @brief Stop the component
     * 
     * This method should be overridden by derived classes to stop the component's operation.
     * It should gracefully clean up resources and terminate any running threads.
     */
    virtual void stop() = 0;
    
    /**
     * @brief Check if the component is running
     * @return true if the component is running, false otherwise
     */
    bool isRunning() const;
    
    /**
     * @brief Get the name of the component
     * @return The component name
     */
    std::string getName() const;
    
    /**
     * @brief Get the component status as a string
     * @return A string describing the component's current status
     */
    virtual std::string getStatus() const;
    
    /**
     * @brief Configure the component with key-value pairs
     * @param config Map of configuration key-value pairs
     */
    virtual void configure(const std::map<std::string, std::string>& config);
    
    /**
     * @brief Get the component's logger
     * @return A shared pointer to the component's logger
     */
    std::shared_ptr<spdlog::logger> getLogger() const;

protected:
    /**
     * @brief Set the running state of the component
     * @param running The new running state
     */
    void setRunning(bool running);
    
    /**
     * @brief Set the component status
     * @param status The new status string
     */
    void setStatus(const std::string& status);
    
    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Component name
    std::string name_;
    
    // Running state
    bool running_;
    
    // Status
    std::string status_;
    
    // Logger
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace af::common