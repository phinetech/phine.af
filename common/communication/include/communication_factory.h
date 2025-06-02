/**
 * @file communication_factory.h
 * @brief Factory for creating communication service instances
 * 
 * This file defines the CommunicationFactory which is responsible for creating
 * appropriate CommunicationService instances based on configuration. It supports
 * different communication technologies (gRPC, direct calls, etc.) and provides
 * a unified interface for obtaining communication services.
 * 
 * The factory pattern allows the application to select the appropriate communication
 * mechanism at runtime based on configuration, without modifying the code that
 * uses the communication services.
 */

 #pragma once

 #include "communication_interface.h"
 #include <memory>
 #include <string>
 #include <unordered_map>
 
 namespace af {
 namespace communication {
 
 /**
  * @brief Communication mechanism types
  */
 enum class CommunicationType {
     GRPC,       ///< gRPC-based communication
     DIRECT,     ///< Direct function call communication
     ZEROMQ,     ///< ZeroMQ-based communication (not implemented yet)
     REST        ///< REST-based communication (not implemented yet)
 };
 
 /**
  * @brief Factory for creating communication service instances
  */
 class CommunicationFactory {
 public:
     /**
      * @brief Create a communication service of the specified type
      * 
      * @param type Type of communication service to create
      * @param service_name Name of the service using this communication
      * @param config Configuration parameters
      * @return CommunicationServicePtr Pointer to the created service
      */
     static CommunicationServicePtr create_service(
         CommunicationType type,
         const std::string& service_name,
         const std::unordered_map<std::string, std::string>& config = {});
 
     /**
      * @brief Create a communication service from a string type
      * 
      * @param type_str String representation of the communication type
      * @param service_name Name of the service using this communication
      * @param config Configuration parameters
      * @return CommunicationServicePtr Pointer to the created service
      */
     static CommunicationServicePtr create_service(
         const std::string& type_str,
         const std::string& service_name,
         const std::unordered_map<std::string, std::string>& config = {});
 
 private:
     // Static class, no instances needed
     CommunicationFactory() = delete;
 };
 
 } // namespace communication
 } // namespace af