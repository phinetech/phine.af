/**
 * @file communication_interface.h
 * @brief Abstract interface for communication between AF components
 * 
 * This file defines the CommunicationService interface, which provides an abstraction
 * layer for different communication mechanisms (gRPC, direct calls, etc.) used
 * between AF components.
 * 
 * The interface supports:
 * - Synchronous request-response communication
 * - Asynchronous message delivery with callbacks
 * - Subscription-based pattern for continuous updates
 * 
 * This abstraction allows AF components to communicate without being tightly coupled
 * to a specific communication technology, making it easier to change or mix
 * technologies as needed.
 */

 #pragma once

 #include "message.h"
 #include <string>
 #include <functional>
 #include <future>
 #include <unordered_map>
 #include <vector>
 
 namespace af {
 namespace communication {
 
 /**
  * @brief Abstract interface for communication services
  * 
  * This interface defines the methods for sending and receiving messages
  * between AF components.
  */
 class CommunicationService {
 public:
     virtual ~CommunicationService() = default;
 
     /**
      * @brief Initialize the communication service
      * 
      * @param service_name Name of this service instance
      * @param config Configuration parameters
      * @return bool True if initialization succeeded
      */
     virtual bool initialize(const std::string& service_name, 
                            const std::unordered_map<std::string, std::string>& config) = 0;
 
     /**
      * @brief Send a message and wait for response
      * 
      * @param destination Target service name
      * @param message Message to send
      * @return MessagePtr Response message or nullptr if failed
      */
     virtual MessagePtr send_request(const std::string& destination, 
                                    const MessagePtr& message) = 0;
 
     /**
      * @brief Send a message asynchronously
      * 
      * @param destination Target service name
      * @param message Message to send
      * @param callback Optional callback for response
      * @return bool True if message was sent successfully
      */
     virtual bool send_async(const std::string& destination, 
                            const MessagePtr& message,
                            const MessageCallback& callback = nullptr) = 0;
 
     /**
      * @brief Register a message handler for a specific message type
      * 
      * @param message_type Type of message to handle
      * @param handler Handler for the message
      * @return bool True if registration succeeded
      */
     virtual bool register_handler(const std::string& message_type, 
                                  const MessageHandlerPtr& handler) = 0;
 
     /**
      * @brief Register a callback for a specific message type
      * 
      * @param message_type Type of message to handle
      * @param callback Callback function for the message
      * @return bool True if registration succeeded
      */
     virtual bool register_callback(const std::string& message_type, 
                                   const MessageCallback& callback) = 0;
 
     /**
      * @brief Subscribe to messages of a specific type from a service
      * 
      * @param source Source service name
      * @param message_type Type of message to subscribe to
      * @param callback Callback for received messages
      * @return std::string Subscription ID or empty string if failed
      */
     virtual std::string subscribe(const std::string& source, 
                                  const std::string& message_type,
                                  const MessageCallback& callback) = 0;
 
     /**
      * @brief Unsubscribe from a subscription
      * 
      * @param subscription_id ID of the subscription to cancel
      * @return bool True if unsubscription succeeded
      */
     virtual bool unsubscribe(const std::string& subscription_id) = 0;
 
     /**
      * @brief Start the communication service
      * 
      * @return bool True if service started successfully
      */
     virtual bool start() = 0;
 
     /**
      * @brief Stop the communication service
      * 
      * @return bool True if service stopped successfully
      */
     virtual bool stop() = 0;
 };
 
 // Type alias for shared service pointers
 using CommunicationServicePtr = std::shared_ptr<CommunicationService>;
 
 } // namespace communication
 } // namespace af