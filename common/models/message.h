/**
 * @file message.h
 * @brief Defines the Message model for internal AF communication
 * 
 * This file provides a C++ implementation of the generic Message structure used for
 * internal communication between AF microservices. This structure aligns with the
 * protobuf InternalMessage definition but is designed for use within C++ components.
 * 
 * The Message struct contains:
 * - message_type: Identifies the type/purpose of the message
 * - correlation_id: Links related messages (e.g., request-response pairs)
 * - payload: Binary data carrying the actual content
 * - metadata: Additional contextual information as key-value pairs
 * 
 * This Message model is used throughout the AF microservices for passing information
 * between components, regardless of whether they use gRPC, direct function calls,
 * or other communication mechanisms internally.
 * 
 * Usage:
 *   auto msg = af::models::Message("QoSRequest", "req-123", 
 *                                 std::vector<uint8_t>{...});
 *   msg.metadata["priority"] = "high";
 *   // Send message to another component
 */

 #pragma once

 #include <string>
 #include <vector>
 #include <unordered_map>
 #include <memory>
 
 namespace af {
 namespace models {
 
 /**
  * @brief Generic message struct for internal AF communication
  */
 struct Message {
     std::string message_type;               // Type of the message
     std::string correlation_id;             // Correlation ID for request-response pattern
     std::vector<uint8_t> payload;           // Binary payload
     std::unordered_map<std::string, std::string> metadata; // Additional metadata
 
     Message() = default;
     
     Message(std::string type, std::string id, std::vector<uint8_t> data) 
         : message_type(std::move(type)), 
           correlation_id(std::move(id)), 
           payload(std::move(data)) {}
           
     Message(std::string type, std::string id, std::vector<uint8_t> data,
             std::unordered_map<std::string, std::string> meta) 
         : message_type(std::move(type)), 
           correlation_id(std::move(id)), 
           payload(std::move(data)),
           metadata(std::move(meta)) {}
 };
 
 // Type alias for shared message pointers
 using MessagePtr = std::shared_ptr<Message>;
 
 } // namespace models
 } // namespace af