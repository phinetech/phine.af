/**
 * @file pcf_gateway.h
 * @brief Abstract interface for PCF (Policy Control Function) interactions
 *
 * This interface decouples business logic from the transport mechanism used
 * to communicate with the 5G PCF. Implementations can use HTTP/2 (via
 * HttpCommunicationService), mock responses for testing, or any future transport.
 */

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <nlohmann/json.hpp>

namespace af {
namespace southbound {

/**
 * @brief Abstract gateway interface for PCF communication
 *
 * Provides a clean contract for PCF application session operations
 * (Npcf_PolicyAuthorization). Implementations handle transport details.
 */
class PcfGateway {
public:
    virtual ~PcfGateway() = default;

    /**
     * @brief Initialize the gateway (establish connections, etc.)
     * @return true if initialization succeeded
     */
    virtual bool initialize() = 0;

    /**
     * @brief Create an application session with the PCF
     * @param app_session_context JSON representation of AppSessionContext
     * @return Pair of success flag and response JSON
     */
    virtual std::pair<bool, nlohmann::json> create_app_session(
        const nlohmann::json& app_session_context) = 0;

    /**
     * @brief Update an existing application session
     * @param app_session_id The PCF application session ID
     * @param update_data JSON patch/update data
     * @return Pair of success flag and response JSON
     */
    virtual std::pair<bool, nlohmann::json> update_app_session(
        const std::string& app_session_id,
        const nlohmann::json& update_data) = 0;

    /**
     * @brief Delete an application session
     * @param app_session_id The PCF application session ID
     * @param delete_data Optional termination/event data
     * @return true if deletion succeeded
     */
    virtual bool delete_app_session(
        const std::string& app_session_id,
        const nlohmann::json& delete_data) = 0;

    /**
     * @brief Get information about an application session
     * @param app_session_id The PCF application session ID
     * @return Pair of success flag and response JSON
     */
    virtual std::pair<bool, nlohmann::json> get_app_session(
        const std::string& app_session_id) = 0;
};

} // namespace southbound
} // namespace af
