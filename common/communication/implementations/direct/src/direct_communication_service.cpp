/**
 * @file direct_communication_service.cpp
 * @brief Implementation of the direct communication service
 * 
 * This file implements the DirectCommunicationService class, which provides
 * in-process communication between AF components using direct function calls.
 * 
 * The implementation maintains a global registry of services and routes messages
 * directly to their handlers, avoiding serialization and network overhead.
 */

 #include "direct_communication_service.h"
 #include "cpp_utils/error.h"
 #include <iostream>
 #include <thread>
 #include <random>
 #include <sstream>
 
 namespace af {
 namespace communication {
 namespace direct {
 
 // Initialize static registry
 DirectCommunicationService::ServiceRegistry DirectCommunicationService::registry_;
 
 DirectCommunicationService::DirectCommunicationService() 
     : is_running_(false) {
 }
 
 DirectCommunicationService::~DirectCommunicationService() {
     stop();
 }
 
 bool DirectCommunicationService::initialize(
     const std::string& service_name, 
     const std::unordered_map<std::string, std::string>& config) {
     
     service_name_ = service_name;
     return true;
 }
 
 MessagePtr DirectCommunicationService::send_request(
     const std::string& destination, 
     const MessagePtr& message) {
     
     if (!message || !is_running_) {
         return nullptr;
     }
 
     // Find the destination service in the registry
     DirectCommunicationService* dest_service = nullptr;
     {
         std::lock_guard<std::mutex> lock(registry_.mutex);
         auto it = registry_.services.find(destination);
         if (it == registry_.services.end()) {
             std::cerr << "Destination service not found: " << destination << std::endl;
             return nullptr;
         }
         dest_service = it->second;
     }
 
     // Create a copy of the message to send
     MessagePtr msg_copy = std::make_shared<Message>(*message);
     
     // Add source information
     msg_copy->metadata["source_service"] = service_name_;
 
     // Send message to destination service
     MessagePtr response = nullptr;
     if (!dest_service->handle_message(msg_copy, response)) {
         return nullptr;
     }
 
     return response;
 }
 
 bool DirectCommunicationService::send_async(
     const std::string& destination, 
     const MessagePtr& message, 
     const MessageCallback& callback) {
     
     if (!message || !is_running_) {
         return false;
     }
 
     // For simplicity, we'll just spawn a thread that calls send_request
     std::thread([this, destination, message, callback]() {
         MessagePtr response = this->send_request(destination, message);
         if (callback && response) {
             callback(response);
         }
     }).detach();
 
     return true;
 }
 
 bool DirectCommunicationService::register_handler(
     const std::string& message_type, 
     const MessageHandlerPtr& handler) {
     
     if (!handler) {
         return false;
     }
 
     std::lock_guard<std::mutex> lock(handlers_mutex_);
     message_handlers_[message_type] = handler;
     return true;
 }
 
 bool DirectCommunicationService::register_callback(
     const std::string& message_type, 
     const MessageCallback& callback) {
     
     if (!callback) {
         return false;
     }
 
     std::lock_guard<std::mutex> lock(handlers_mutex_);
     message_callbacks_[message_type] = callback;
     return true;
 }
 
 std::string DirectCommunicationService::subscribe(
     const std::string& source, 
     const std::string& message_type, 
     const MessageCallback& callback) {
     
     if (!callback || !is_running_) {
         return "";
     }
 
     // Generate a unique subscription ID
     std::string subscription_id = generate_subscription_id();
 
     // Create subscription
     Subscription subscription;
     subscription.source = source;
     subscription.message_type = message_type;
     subscription.callback = callback;
 
     // Store subscription
     {
         std::lock_guard<std::mutex> lock(subscriptions_mutex_);
         subscriptions_[subscription_id] = subscription;
     }
 
     // Find the source service
     DirectCommunicationService* source_service = nullptr;
     {
         std::lock_guard<std::mutex> lock(registry_.mutex);
         auto it = registry_.services.find(source);
         if (it == registry_.services.end()) {
             // Source not found yet, will try to connect when it becomes available
             return subscription_id;
         }
         source_service = it->second;
     }
 
     // TODO: Set up notification mechanism for direct subscriptions
     // For a complete implementation, we would need to maintain a mapping of
     // subscribers in the source service
 
     return subscription_id;
 }
 
 bool DirectCommunicationService::unsubscribe(const std::string& subscription_id) {
     std::lock_guard<std::mutex> lock(subscriptions_mutex_);
     
     auto it = subscriptions_.find(subscription_id);
     if (it == subscriptions_.end()) {
         return false;
     }
 
     // TODO: Remove notification mechanism for direct subscriptions
 
     // Remove subscription
     subscriptions_.erase(it);
     return true;
 }
 
 bool DirectCommunicationService::start() {
     if (is_running_) {
         return true;  // Already running
     }
 
     // Register this service in the global registry
     {
         std::lock_guard<std::mutex> lock(registry_.mutex);
         registry_.services[service_name_] = this;
     }
 
     is_running_ = true;
     return true;
 }
 
 bool DirectCommunicationService::stop() {
     if (!is_running_) {
         return true;  // Already stopped
     }
 
     // Unregister this service from the global registry
     {
         std::lock_guard<std::mutex> lock(registry_.mutex);
         registry_.services.erase(service_name_);
     }
 
     // Clear handlers and subscriptions
     {
         std::lock_guard<std::mutex> lock(handlers_mutex_);
         message_handlers_.clear();
         message_callbacks_.clear();
     }
 
     {
         std::lock_guard<std::mutex> lock(subscriptions_mutex_);
         subscriptions_.clear();
     }
 
     is_running_ = false;
     return true;
 }
 
 std::string DirectCommunicationService::generate_subscription_id() {
     // Generate a random UUID-like string for subscription IDs
     static std::random_device rd;
     static std::mt19937 gen(rd());
     static std::uniform_int_distribution<> dis(0, 15);
     static const char* hex_chars = "0123456789abcdef";
 
     std::stringstream ss;
     ss << service_name_ << "-sub-";
     for (int i = 0; i < 16; ++i) {
         ss << hex_chars[dis(gen)];
     }
 
     return ss.str();
 }
 
 bool DirectCommunicationService::handle_message(
     const MessagePtr& message,
     MessagePtr& response) {
     
     if (!message || !is_running_) {
         return false;
     }
 
     // Process message based on registered handlers/callbacks
     bool handled = false;
     
     // Try handlers first
     {
         std::lock_guard<std::mutex> lock(handlers_mutex_);
         auto handler_it = message_handlers_.find(message->message_type);
         if (handler_it != message_handlers_.end()) {
             response = handler_it->second->handle_message(message);
             handled = true;
         } else {
             auto callback_it = message_callbacks_.find(message->message_type);
             if (callback_it != message_callbacks_.end()) {
                 response = callback_it->second(message);
                 handled = true;
             }
         }
     }
 
     if (!handled) {
         std::cerr << "No handler registered for message type: " 
                   << message->message_type << std::endl;
         return false;
     }
 
     if (!response) {
         // Create empty response if handler didn't provide one
         response = std::make_shared<Message>();
         response->correlation_id = message->correlation_id;
     }
 
     return true;
 }
 
 } // namespace direct
 } // namespace communication
 } // namespace af