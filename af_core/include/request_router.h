/**
 * @file request_router.h
 * @brief Routes incoming requests to appropriate handlers
 *
 * This class is responsible for routing incoming messages to the appropriate
 * handler based on the message type.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <spdlog/spdlog.h>
#include "common/communication/include/message.h"

namespace af {
namespace core {

// Forward declaration
class AfOrchestrator;

/**
 * @brief Type alias for message handler function
 */
using MessageHandlerFunc = std::function<af::communication::MessagePtr(const af::communication::MessagePtr&)>;

/**
 * @brief Routes messages to appropriate handlers
 */
class RequestRouter {
public:
    /**
     * @brief Constructor
     */
    RequestRouter();

    /**
     * @brief Destructor
     */
    ~RequestRouter();

    /**
     * @brief Initialize the router
     * @param orchestrator Pointer to the orchestrator
     */
    void initialize(AfOrchestrator* orchestrator);

    /**
     * @brief Register a handler for a specific message type
     * @param message_type Type of message to handle
     * @param handler Handler function
     */
    void register_handler(const std::string& message_type,
                          const MessageHandlerFunc& handler);

    /**
     * @brief Route a message to the appropriate handler
     * @param message The message to route
     * @return Response message
     */
    af::communication::MessagePtr route_message(
        const af::communication::MessagePtr& message);

    /**
     * @brief Initialize the component's logger
     * @param log_level The log level to use
     */
    void initializeLogger(spdlog::level::level_enum log_level = spdlog::level::info);

private:
    // Reference to orchestrator
    AfOrchestrator* orchestrator_;

    // Message type to handler mapping
    std::unordered_map<std::string, MessageHandlerFunc> handlers_;

    // Default handler for unknown message types
    MessageHandlerFunc default_handler_;

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

    /**
     * @brief Create a default handler for unknown message types
     * @return Handler function
     */
    MessageHandlerFunc create_default_handler();
};

} // namespace core
} // namespace af