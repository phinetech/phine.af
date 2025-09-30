/**
 * @file pcf_handler.h
 * @brief Handler for interactions with the 5G PCF
 *
 * This component handles communication with the Policy Control Function (PCF)
 * using the Npcf_PolicyAuthorization API to create, update, and delete
 * application sessions and influence 5G QoS policies.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>
#include "../common/component/include/af_component.h"
#include "../common/communication/include/communication_interface.h"
#include "request_router.h"
#include "pcc_rule_manager.h"
#include "pcf_client_wrapper.h"

#include "AppSessionContext.h"
#include "AppSessionContextReqData.h"
#include "MediaComponent.h"

namespace af {
namespace southbound {

// Forward declaration
class RequestRouter;

/**
 * @brief Handler for PCF interactions
 *
 * Responsible for managing communication with the 5G Policy Control Function (PCF)
 * to influence QoS, traffic steering, and charging policies.
 */
class PcfHandler : public af::common::AfComponent {
public:
    /**
     * @brief Constructor
     * @param config_path Path to configuration file
     */
    PcfHandler(const std::string& config_path);
    
    /**
     * @brief Destructor
     */
    ~PcfHandler();
    
    /**
     * @brief Initialize the PCF handler
     */
    void initialize() override;
    
    /**
     * @brief Start the PCF handler
     */
    void start() override;
    
    /**
     * @brief Stop the PCF handler
     */
    void stop() override;
    
    /**
     * @brief Process a message from any source
     * @param message The incoming message
     * @return Response message
     */
    af::communication::MessagePtr process_message(
        const af::communication::MessagePtr& message);

    /**
     * @brief Create an application session with the PCF
     * @param app_session_data Application session data
     * @return Success response with session details or error
     */
    af::communication::MessagePtr create_app_session(
        const af::communication::MessagePtr& app_session_data);
    
    /**
     * @brief Update an existing application session
     * @param update_data Update data including session ID
     * @return Success response or error
     */
    af::communication::MessagePtr update_app_session(
        const af::communication::MessagePtr& update_data);
    
    /**
     * @brief Delete an application session
     * @param delete_data Delete request including session ID
     * @return Success response or error
     */
    af::communication::MessagePtr delete_app_session(
        const af::communication::MessagePtr& delete_data);
    
    /**
     * @brief Get information about an existing application session
     * @param get_data Get request including session ID
     * @return Session information or error
     */
    af::communication::MessagePtr get_app_session(
        const af::communication::MessagePtr& get_data);
    
    /**
     * @brief Handle a notification from the PCF
     * @param notification_data Notification data
     * @return Success response
     */
    af::communication::MessagePtr handle_notification(
        const af::communication::MessagePtr& notification_data);

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Configuration
    std::string config_path_;
    std::string pcf_base_url_;
    bool use_tls_;
    std::string api_version_;
    
    // Components
    std::shared_ptr<PcfClientWrapper> pcf_client_;
    std::shared_ptr<PccRuleManager> pcc_rule_manager_;
    
    // Communication service for talking to the AF Core
    std::shared_ptr<af::communication::CommunicationService> core_comm_;
    
    // Request router for handling incoming messages
    std::shared_ptr<RequestRouter> request_router_;

    // Message handler for incoming messages
    class PcfMessageHandler;
    std::shared_ptr<PcfMessageHandler> message_handler_;
    
    // Application session tracking
    struct AppSessionInfo {
        std::string app_session_id;
        std::string app_session_context;
        std::string supi;
        std::string dnn;
        std::string ipv4_address;
        std::string ipv6_prefix;
        std::vector<std::string> media_components;
        bool active;
    };
    
    std::unordered_map<std::string, AppSessionInfo> app_sessions_;
    std::mutex app_sessions_mutex_;
    
    // Logger
    std::shared_ptr<spdlog::logger> logger_;
    
    /**
     * @brief Load configuration from file
     */
    void load_config();
    
    /**
     * @brief Initialize communication with AF Core
     */
    void initialize_communication();
    
    /**
     * @brief Register message handlers
     */
    void register_handlers();
    
    /**
     * @brief Store application session information
     * @param session_id Session ID
     * @param session_info Session information
     */
    void store_app_session(const std::string& session_id, const AppSessionInfo& session_info);
    
    /**
     * @brief Get application session information
     * @param session_id Session ID
     * @return Session information or nullptr if not found
     */
    std::shared_ptr<AppSessionInfo> get_app_session_info(const std::string& session_id);
    
    /**
     * @brief Remove application session information
     * @param session_id Session ID
     * @return true if session was found and removed
     */
    bool remove_app_session(const std::string& session_id);
    
    /**
     * @brief Forward a notification to the AF Core
     * @param notification_type Type of notification
     * @param notification_data Notification data
     */
    void forward_notification(const std::string& notification_type, 
                              const nlohmann::json& notification_data);
};

} // namespace southbound
} // namespace af