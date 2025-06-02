/**
 * @file result.h
 * @brief Provides a Result type for handling operation outcomes with success or error states
 * 
 * This file implements a Result<T, E> template class that represents either a successful
 * result of type T or an error of type E. This approach enables more explicit error
 * handling without relying on exceptions, which can be useful in performance-critical
 * code paths or when a more functional programming style is preferred.
 * 
 * The Result type is inspired by Rust's Result enum and provides a type-safe way to
 * handle operations that can fail, forcing the caller to handle both success and error cases.
 * 
 * Usage:
 *   Result<int, std::string> divide(int a, int b) {
 *     if (b == 0) {
 *       return "Division by zero";
 *     }
 *     return a / b;
 *   }
 * 
 *   auto result = divide(10, 2);
 *   if (result.is_ok()) {
 *     int value = result.value();
 *   } else {
 *     std::string error = result.error();
 *   }
 */

 #pragma once

 #include <variant>
 #include <optional>
 #include <utility>
 
 namespace af {
 namespace utils {
 
 template <typename T, typename E>
 class Result {
 public:
     Result(const T& value) : result_(value) {}
     Result(T&& value) : result_(std::move(value)) {}
     Result(const E& error) : result_(error) {}
     Result(E&& error) : result_(std::move(error)) {}
 
     bool is_ok() const { return std::holds_alternative<T>(result_); }
     bool is_error() const { return std::holds_alternative<E>(result_); }
 
     const T& value() const { return std::get<T>(result_); }
     const E& error() const { return std::get<E>(result_); }
 
     T& value() { return std::get<T>(result_); }
     E& error() { return std::get<E>(result_); }
 
 private:
     std::variant<T, E> result_;
 };
 
 } // namespace utils
 } // namespace af