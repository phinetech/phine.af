/**
 * @file message.h
 * @brief Defines the message format used for communication between AF components
 * 
 * This file provides a common message format for all communication mechanisms
 * in the AF microservice. It reuses the Message model defined in common/models
 * for consistency across the application.
 * 
 * The MessageHandler class provides a callback mechanism for asynchronous message
 * processing, allowing components to register handlers for different message types.
 */

 #pragma once

 #include "models/message.h"
 #include <functional>
 #include <memory>
 #include <string>
 
 namespace af {
 namespace communication {
 
 // Reuse the Message model from common/models
 using Message = models::Message;
 using MessagePtr = models::MessagePtr;
 
 /**
  * @brief Interface for message handlers
  * 
  * This interface defines the callback mechanism for processing messages
  * asynchronously. Components implement this interface to handle messages
  * of specific types.
  */
 class MessageHandler {
 public:
     virtual ~MessageHandler() = default;
 
     /**
      * @brief Handle a message
      * 
      * @param message The message to handle
      * @return MessagePtr Optional response message
      */
     virtual MessagePtr handle_message(const MessagePtr& message) = 0;
 };
 
 // Type definitions for callbacks and handler storage
 using MessageHandlerPtr = std::shared_ptr<MessageHandler>;
 using MessageCallback = std::function<MessagePtr(const MessagePtr&)>;
 
 } // namespace communication
 } // namespace af