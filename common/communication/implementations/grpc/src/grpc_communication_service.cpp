/**
 * @file grpc_communication_service.cpp
 * @brief Implementation of the gRPC communication service
 * 
 * This file implements the GrpcCommunicationService class, which provides
 * communication between AF components using gRPC as the transport mechanism.
 * 
 * The implementation includes:
 * - A gRPC server that handles incoming messages
 * - gRPC client connections to other services
 * - Message routing based on registered handlers and callbacks
 * - Subscription management for streaming updates
 */

 #include "grpc_communication_service.h"
 #include "cpp_utils/error.h"
 #include <grpcpp/grpcpp.h>
 #include <chrono>
 #include <random>
 #include <sstream>
 #include <iostream>
 
 // Include generated protobuf/gRPC code
 #include "message.grpc.pb.h"
 
 namespace af {
 namespace communication {
 namespace grpc {
 
 // Implementation of the internal gRPC service
 class GrpcCommunicationService::InternalGrpcServiceImpl final 
     : public af::proto::InternalCommunication::Service {
 public:
     InternalGrpcServiceImpl(GrpcCommunicationService* parent)
         : parent_(parent) {}
 
     ::grpc::Status SendMessage(
         ::grpc::ServerContext* context,
         const af::proto::InternalMessage* request,
         af::proto::InternalMessage* response) override {
         
         // Convert protobuf message to internal Message format
         MessagePtr req_msg = std::make_shared<Message>();
         req_msg->message_type = request->message_type();
         req_msg->correlation_id = request->correlation_id();
         req_msg->payload = std::vector<uint8_t>(
             request->payload().begin(), request->payload().end());
         
         // Copy metadata
         for (const auto& entry : request->metadata()) {
             req_msg->metadata[entry.first] = entry.second;
         }
 
         // Process message based on registered handlers/callbacks
         MessagePtr resp_msg = nullptr;
         
         // Try handlers first
         bool handled = false;
         {
             std::lock_guard<std::mutex> lock(parent_->handlers_mutex_);
             auto handler_it = parent_->message_handlers_.find(req_msg->message_type);
             if (handler_it != parent_->message_handlers_.end()) {
                 resp_msg = handler_it->second->handle_message(req_msg);
                 handled = true;
             } else {
                 auto callback_it = parent_->message_callbacks_.find(req_msg->message_type);
                 if (callback_it != parent_->message_callbacks_.end()) {
                     resp_msg = callback_it->second(req_msg);
                     handled = true;
                 }
             }
         }
 
         if (!handled) {
             return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, 
                 "No handler registered for message type: " + req_msg->message_type);
         }
 
         if (!resp_msg) {
             // Create empty response if handler didn't provide one
             resp_msg = std::make_shared<Message>();
             resp_msg->correlation_id = req_msg->correlation_id;
         }
 
         // Convert response to protobuf format
         response->set_message_type(resp_msg->message_type);
         response->set_correlation_id(resp_msg->correlation_id);
         response->set_payload(resp_msg->payload.data(), resp_msg->payload.size());
         
         // Copy metadata
         for (const auto& entry : resp_msg->metadata) {
             (*response->mutable_metadata())[entry.first] = entry.second;
         }
 
         return ::grpc::Status::OK;
     }
 
     ::grpc::Status StreamMessages(
         ::grpc::ServerContext* context,
         ::grpc::ServerReaderWriter<af::proto::InternalMessage, af::proto::InternalMessage>* stream) override {
         
         // For simplicity, we'll implement a basic version here
         // A full implementation would handle bidirectional streaming properly
         
         af::proto::InternalMessage request;
         while (stream->Read(&request)) {
             // Convert and process message similar to SendMessage
             MessagePtr req_msg = std::make_shared<Message>();
             req_msg->message_type = request.message_type();
             req_msg->correlation_id = request.correlation_id();
             req_msg->payload = std::vector<uint8_t>(
                 request.payload().begin(), request.payload().end());
             
             for (const auto& entry : request.metadata()) {
                 req_msg->metadata[entry.first] = entry.second;
             }
 
             // Process message and get response
             MessagePtr resp_msg = nullptr;
             
             {
                 std::lock_guard<std::mutex> lock(parent_->handlers_mutex_);
                 auto handler_it = parent_->message_handlers_.find(req_msg->message_type);
                 if (handler_it != parent_->message_handlers_.end()) {
                     resp_msg = handler_it->second->handle_message(req_msg);
                 } else {
                     auto callback_it = parent_->message_callbacks_.find(req_msg->message_type);
                     if (callback_it != parent_->message_callbacks_.end()) {
                         resp_msg = callback_it->second(req_msg);
                     }
                 }
             }
 
             if (!resp_msg) {
                 resp_msg = std::make_shared<Message>();
                 resp_msg->correlation_id = req_msg->correlation_id;
                 resp_msg->message_type = "error.no_handler";
                 std::string error = "No handler for message type: " + req_msg->message_type;
                 resp_msg->payload = std::vector<uint8_t>(error.begin(), error.end());
             }
 
             // Send response
             af::proto::InternalMessage response;
             response.set_message_type(resp_msg->message_type);
             response.set_correlation_id(resp_msg->correlation_id);
             response.set_payload(resp_msg->payload.data(), resp_msg->payload.size());
             
             for (const auto& entry : resp_msg->metadata) {
                 (*response.mutable_metadata())[entry.first] = entry.second;
             }
 
             if (!stream->Write(response)) {
                 return ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, 
                     "Failed to write response to stream");
             }
         }
 
         return ::grpc::Status::OK;
     }
 
 private:
     GrpcCommunicationService* parent_;
 };
 
 // GrpcCommunicationService implementation
 
 GrpcCommunicationService::GrpcCommunicationService()
     : is_running_(false) {
 }
 
 GrpcCommunicationService::~GrpcCommunicationService() {
     stop();
 }
 
 bool GrpcCommunicationService::initialize(
     const std::string& service_name,
     const std::unordered_map<std::string, std::string>& config) {
     
     service_name_ = service_name;
 
     // Extract configuration parameters
     std::string server_address = "0.0.0.0";  // Default to all interfaces
     int server_port = 0;  // Default to auto-assigned port
     int max_threads = 4;  // Default thread count
 
     auto it = config.find("server_address");
     if (it != config.end()) {
         server_address = it->second;
     }
 
     it = config.find("server_port");
     if (it != config.end()) {
         try {
             server_port = std::stoi(it->second);
         } catch (const std::exception& e) {
             return false;
         }
     }
 
     it = config.find("max_threads");
     if (it != config.end()) {
         try {
             max_threads = std::stoi(it->second);
         } catch (const std::exception& e) {
             return false;
         }
     }
 
     // Create server address string
     server_address_ = server_address + ":" + std::to_string(server_port);
 
     // Create service implementation
     service_impl_ = std::make_unique<InternalGrpcServiceImpl>(this);
 
     // Create server builder
     ::grpc::ServerBuilder builder;
     builder.AddListeningPort(server_address_, ::grpc::InsecureServerCredentials(), &server_port);
     builder.RegisterService(service_impl_.get());
     builder.SetMaxReceiveMessageSize(-1);  // Unlimited message size
     builder.SetMaxSendMessageSize(-1);     // Unlimited message size
 
     // Set up thread pool
     if (max_threads > 0) {
         builder.SetSyncServerOption(::grpc::ServerBuilder::SyncServerOption::NUM_CQS, max_threads);
         builder.SetSyncServerOption(::grpc::ServerBuilder::SyncServerOption::MIN_POLLERS, max_threads);
         builder.SetSyncServerOption(::grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, max_threads);
     }
 
     // Build the server
     server_ = builder.BuildAndStart();
     if (!server_) {
         return false;
     }
 
     // Update server address with actual port if it was auto-assigned
     if (server_port == 0) {
         server_address_ = server_address + ":" + std::to_string(server_port);
     }
 
     return true;
 }
 
 MessagePtr GrpcCommunicationService::send_request(
     const std::string& destination,
     const MessagePtr& message) {
     
     if (!message) {
         return nullptr;
     }
 
     try {
         // Get or create connection to destination
         ClientConnection& connection = get_or_create_connection(destination);
 
         // Create stub
         auto stub = af::proto::InternalCommunication::NewStub(connection.channel);
 
         // Create gRPC request
         af::proto::InternalMessage request;
         request.set_message_type(message->message_type);
         request.set_correlation_id(message->correlation_id);
         request.set_payload(message->payload.data(), message->payload.size());
 
         // Add metadata
         for (const auto& entry : message->metadata) {
             (*request.mutable_metadata())[entry.first] = entry.second;
         }
 
         // Set up context with timeout
         ::grpc::ClientContext context;
         std::chrono::system_clock::time_point deadline =
             std::chrono::system_clock::now() + std::chrono::seconds(30);
         context.set_deadline(deadline);
 
         // Send request
         af::proto::InternalMessage response;
         ::grpc::Status status = stub->SendMessage(&context, request, &response);
 
         if (!status.ok()) {
             // Handle error
             std::cerr << "gRPC error: " << status.error_message() << std::endl;
             return nullptr;
         }
 
         // Convert response to internal format
         MessagePtr resp_msg = std::make_shared<Message>();
         resp_msg->message_type = response.message_type();
         resp_msg->correlation_id = response.correlation_id();
         resp_msg->payload = std::vector<uint8_t>(
             response.payload().begin(), response.payload().end());
 
         // Copy metadata
         for (const auto& entry : response.metadata()) {
             resp_msg->metadata[entry.first] = entry.second;
         }
 
         return resp_msg;
     } catch (const std::exception& e) {
         std::cerr << "Exception in send_request: " << e.what() << std::endl;
         return nullptr;
     }
 }
 
 bool GrpcCommunicationService::send_async(
     const std::string& destination,
     const MessagePtr& message,
     const MessageCallback& callback) {
     
     if (!message) {
         return false;
     }
 
     // For simplicity, we'll implement this as a new thread that calls send_request
     // A more efficient implementation would use gRPC's async API
     std::thread([this, destination, message, callback]() {
         MessagePtr response = this->send_request(destination, message);
         if (callback && response) {
             callback(response);
         }
     }).detach();
 
     return true;
 }
 
 bool GrpcCommunicationService::register_handler(
     const std::string& message_type,
     const MessageHandlerPtr& handler) {
     
     if (!handler) {
         return false;
     }
 
     std::lock_guard<std::mutex> lock(handlers_mutex_);
     message_handlers_[message_type] = handler;
     return true;
 }
 
 bool GrpcCommunicationService::register_callback(
     const std::string& message_type,
     const MessageCallback& callback) {
     
     if (!callback) {
         return false;
     }
 
     std::lock_guard<std::mutex> lock(handlers_mutex_);
     message_callbacks_[message_type] = callback;
     return true;
 }
 
 std::string GrpcCommunicationService::subscribe(
     const std::string& source,
     const std::string& message_type,
     const MessageCallback& callback) {
     
     if (!callback) {
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
 
     // TODO: Set up streaming connection to source
     // This would involve creating a gRPC streaming call and handling responses
 
     return subscription_id;
 }
 
 bool GrpcCommunicationService::unsubscribe(const std::string& subscription_id) {
     std::lock_guard<std::mutex> lock(subscriptions_mutex_);
     
     auto it = subscriptions_.find(subscription_id);
     if (it == subscriptions_.end()) {
         return false;
        }

        // TODO: Cancel any active streaming calls for this subscription
    
        // Remove subscription
        subscriptions_.erase(it);
        return true;
    }
    
    bool GrpcCommunicationService::start() {
        if (is_running_) {
            return true;  // Already running
        }
    
        if (!server_) {
            return false;  // Server not initialized
        }
    
        // Start server in a separate thread
        is_running_ = true;
        server_thread_ = std::thread([this]() {
            // This will block until stop() is called
            server_->Wait();
        });
    
        return true;
    }
    
    bool GrpcCommunicationService::stop() {
        if (!is_running_) {
            return true;  // Already stopped
        }
    
        is_running_ = false;
    
        // Shutdown server and wait for it to complete
        if (server_) {
            server_->Shutdown();
        }
    
        // Wait for server thread to exit
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    
        // Close all client connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            client_connections_.clear();
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
    
        return true;
    }
    
    GrpcCommunicationService::ClientConnection& GrpcCommunicationService::get_or_create_connection(
        const std::string& destination) {
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        auto it = client_connections_.find(destination);
        if (it != client_connections_.end()) {
            return it->second;
        }
    
        // Create new connection
        ClientConnection connection;
        connection.channel = ::grpc::CreateChannel(
            destination, ::grpc::InsecureChannelCredentials());
    
        // Store and return the new connection
        auto result = client_connections_.emplace(destination, std::move(connection));
        return result.first->second;
    }
    
    std::string GrpcCommunicationService::generate_subscription_id() {
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
    
    } // namespace grpc
    } // namespace communication
    } // namespace af