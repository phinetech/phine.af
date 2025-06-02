/**
 * @file grpc_communication_service.h
 * @brief gRPC implementation of the communication service
 * 
 * This file defines the GrpcCommunicationService class, which implements the
 * CommunicationService interface using gRPC. It provides communication between
 * AF components using gRPC as the transport mechanism.
 * 
 * The implementation includes a gRPC server for receiving messages and a set of
 * gRPC clients for sending messages to other services. It maps abstract message
 * types to gRPC service methods and handles serialization/deserialization.
 */

 #pragma once

 #include "communication_interface.h"
 #include <grpcpp/grpcpp.h>
 #include <thread>
 #include <mutex>
 #include <condition_variable>
 #include <atomic>
 #include <unordered_map>
 #include <string>
 
 namespace af {
 namespace communication {
 namespace grpc {
 
 /**
  * @brief gRPC implementation of the communication service
  */
 class GrpcCommunicationService : public CommunicationService {
 public:
     GrpcCommunicationService();
     ~GrpcCommunicationService() override;
 
     /**
      * @brief Initialize the gRPC communication service
      * 
      * @param service_name Name of this service instance
      * @param config Configuration parameters including:
      *        - server_address: Address to bind the server (default: 0.0.0.0)
      *        - server_port: Port to bind the server (default: auto-assigned)
      *        - max_threads: Maximum number of server threads (default: 4)
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
      * @brief Start the gRPC server
      * 
      * @return bool True if server started successfully
      */
     bool start() override;
 
     /**
      * @brief Stop the gRPC server
      * 
      * @return bool True if server stopped successfully
      */
     bool stop() override;
 
 private:
     // Internal gRPC service implementation (will be implemented as nested class)
     class InternalGrpcServiceImpl;
     std::unique_ptr<InternalGrpcServiceImpl> service_impl_;
 
     // gRPC server and related components
     std::unique_ptr<::grpc::Server> server_;
     std::string server_address_;
     std::thread server_thread_;
     std::atomic<bool> is_running_;
 
     // Client connections to other services
     struct ClientConnection {
         std::shared_ptr<::grpc::Channel> channel;
         // We'll add stub and other client-specific data here
     };
     std::unordered_map<std::string, ClientConnection> client_connections_;
     std::mutex connections_mutex_;
 
     // Message handlers and callbacks
     std::unordered_map<std::string, MessageHandlerPtr> message_handlers_;
     std::unordered_map<std::string, MessageCallback> message_callbacks_;
     std::mutex handlers_mutex_;
 
     // Subscription management
     struct Subscription {
         std::string source;
         std::string message_type;
         MessageCallback callback;
         // We'll add stream and other subscription-specific data here
     };
     std::unordered_map<std::string, Subscription> subscriptions_;
     std::mutex subscriptions_mutex_;
 
     // Service identity
     std::string service_name_;
 
     // Internal helper methods
     ClientConnection& get_or_create_connection(const std::string& destination);
     std::string generate_subscription_id();
 };
 
 } // namespace grpc
 } // namespace communication
 } // namespace af