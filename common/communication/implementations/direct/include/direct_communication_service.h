/**
 * @file direct_communication_service.h
 * @brief Direct function call implementation of the communication service
 * 
 * This file defines the DirectCommunicationService class, which implements the
 * CommunicationService interface using direct function calls. This implementation
 * is useful for in-process communication between components, avoiding the
 * overhead of network-based communication.
 * 
 * The direct implementation maintains a global registry of services to route
 * messages to the appropriate destination within the same process.
 */

 #pragma once

 #include "communication_interface.h"
 #include <mutex>
 #include <unordered_map>
 #include <string>
 #include <memory>
 
 namespace af {
 namespace communication {
 namespace direct {
 
 /**
  * @brief Direct function call implementation of the communication service
  */
 class DirectCommunicationService : public CommunicationService {
 public:
     DirectCommunicationService();
     ~DirectCommunicationService() override;
 
     /**
      * @brief Initialize the direct communication service
      * 
      * @param service_name Name of this service instance
      * @param config Configuration parameters (not used for direct communication)
      * @return bool True if initialization succeeded
      */
     bool initialize(const std::string& service_name, 
                    const std::unordered_map<std::string, std::string>& config) override;
 
     /**
      * @brief Send a message and wait for response
      * 
      * @param destination Target service name
      * @param message Message to send
      * @return MessagePtr Response message or nullptr if failed
      */
     MessagePtr send_request(const std::string& destination, 
                            const MessagePtr& message) override;
 
     /**
      * @brief Send a message asynchronously
      * 
      * @param destination Target service name
      * @param message Message to send
      * @param callback Optional callback for response
      * @return bool True if message was sent successfully
      */
     bool send_async(const std::string& destination, 
                    const MessagePtr& message,
                    const MessageCallback& callback = nullptr) override;
 
     /**
      * @brief Register a message handler for a specific message type
      * 
      * @param message_type Type of message to handle
      * @param handler Handler for the message
      * @return bool True if registration succeeded
      */
     bool register_handler(const std::string& message_type, 
                          const MessageHandlerPtr& handler) override;
 
     /**
      * @brief Register a callback for a specific message type
      * 
      * @param message_type Type of message to handle
      * @param callback Callback function for the message
      * @return bool True if registration succeeded
      */
     bool register_callback(const std::string& message_type, 
                           const MessageCallback& callback) override;
 
     /**
      * @brief Subscribe to messages of a specific type from a service
      * 
      * @param source Source service name
      * @param message_type Type of message to subscribe to
      * @param callback Callback for received messages
      * @return std::string Subscription ID or empty string if failed
      */
     std::string subscribe(const std::string& source, 
                          const std::string& message_type,
                          const MessageCallback& callback) override;
 
     /**
      * @brief Unsubscribe from a subscription
      * 
      * @param subscription_id ID of the subscription to cancel
      * @return bool True if unsubscription succeeded
      */
     bool unsubscribe(const std::string& subscription_id) override;
 
     /**
      * @brief Start the direct communication service
      * 
      * @return bool True if service started successfully
      */
     bool start() override;
 
     /**
      * @brief Stop the direct communication service
      * 
      * @return bool True if service stopped successfully
      */
     bool stop() override;
 
 private:
     // Static service registry shared by all instances
     struct ServiceRegistry {
         std::unordered_map<std::string, DirectCommunicationService*> services;
         std::mutex mutex;
     };
     static ServiceRegistry registry_;
 
     // Service identity
     std::string service_name_;
     bool is_running_;
 
     // Message handlers and callbacks
     std::unordered_map<std::string, MessageHandlerPtr> message_handlers_;
     std::unordered_map<std::string, MessageCallback> message_callbacks_;
     std::mutex handlers_mutex_;
 
     // Subscription management
     struct Subscription {
         std::string source;
         std::string message_type;
         MessageCallback callback;
     };
     std::unordered_map<std::string, Subscription> subscriptions_;
     std::mutex subscriptions_mutex_;
 
     // Helper methods
     std::string generate_subscription_id();
     bool handle_message(const MessagePtr& message, MessagePtr& response);
 };
 
 } // namespace direct
 } // namespace communication
 } // namespace af