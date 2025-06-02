/**
 * @file error.h
 * @brief Error handling utilities for the AF microservice application
 * 
 * This file contains custom exception classes that provide structured error handling
 * throughout the AF microservice. These error classes allow for consistent error 
 * propagation and handling, with specific error types for different failure scenarios.
 * 
 * The hierarchy starts with a base AFError class and extends to specific error types
 * like ConfigurationError, CommunicationError, and ValidationError.
 * 
 * Usage:
 *   try {
 *     // Code that might throw
 *     if (invalid_config) {
 *       throw ConfigurationError("Missing required configuration parameter");
 *     }
 *   } catch (const ConfigurationError& e) {
 *     // Handle configuration error
 *   } catch (const AFError& e) {
 *     // Handle any AF error
 *   }
 */

 #pragma once

 #include <exception>
 #include <string>
 #include <stdexcept>
 
 namespace af {
 namespace utils {
 
 class AFError : public std::runtime_error {
 public:
     explicit AFError(const std::string& message) : std::runtime_error(message) {}
     explicit AFError(const char* message) : std::runtime_error(message) {}
 };
 
 class ConfigurationError : public AFError {
 public:
     explicit ConfigurationError(const std::string& message) : AFError(message) {}
 };
 
 class CommunicationError : public AFError {
 public:
     explicit CommunicationError(const std::string& message) : AFError(message) {}
 };
 
 class ValidationError : public AFError {
 public:
     explicit ValidationError(const std::string& message) : AFError(message) {}
 };
 
 } // namespace utils
 } // namespace af