/**
 * @file af_config.h
 * @brief Configuration management for the AF microservice
 * 
 * This file defines the Configuration class which provides a centralized way to
 * access configuration parameters throughout the AF microservice. It supports
 * loading configuration from YAML files, environment variables, and command-line
 * arguments, with a cascading override system.
 * 
 * The configuration system uses a hierarchical structure with dot notation for
 * accessing nested parameters (e.g., "database.host").
 * 
 * Example usage:
 *   auto config = af::config::Configuration::load("config.yaml");
 *   std::string host = config->get<std::string>("database.host", "localhost");
 *   int port = config->get<int>("database.port", 5432);
 */

 #pragma once

 #include <memory>
 #include <string>
 #include <vector>
 #include <unordered_map>
 #include <any>
 #include <typeindex>
 #include <functional>
 #include <optional>
 
 namespace af {
 namespace config {
 
 /**
  * @brief Configuration management class
  * 
  * This class provides access to configuration parameters loaded from YAML files,
  * environment variables, and command-line arguments.
  */
 class Configuration {
 public:
     /**
      * @brief Destructor
      */
     ~Configuration();
 
     /**
      * @brief Load configuration from a YAML file
      * 
      * @param file_path Path to the YAML configuration file
      * @return std::shared_ptr<Configuration> Shared pointer to the loaded configuration
      */
     static std::shared_ptr<Configuration> load(const std::string& file_path);
 
     /**
      * @brief Load configuration from a YAML file and command-line arguments
      * 
      * @param file_path Path to the YAML configuration file
      * @param argc Argument count from main()
      * @param argv Argument values from main()
      * @return std::shared_ptr<Configuration> Shared pointer to the loaded configuration
      */
     static std::shared_ptr<Configuration> load(const std::string& file_path, int argc, char** argv);
 
     /**
      * @brief Get a configuration value of the specified type
      * 
      * @tparam T The type of the configuration value
      * @param key The configuration key (with dot notation for nested values)
      * @param default_value The default value to return if the key is not found
      * @return T The configuration value or the default value if not found
      */
     template <typename T>
     T get(const std::string& key, const T& default_value = T()) const {
         auto value = get_value(key);
         if (!value.has_value()) {
             return default_value;
         }
 
         try {
             if constexpr (std::is_same_v<T, std::string>) {
                 // Special handling for string type
                 return convert_to_string(value.value());
             } else {
                 return std::any_cast<T>(value.value());
             }
         } catch (const std::bad_any_cast&) {
             // Try to convert the value to the requested type
             return convert_value<T>(value.value(), default_value);
         }
     }
 
     /**
      * @brief Check if a configuration key exists
      * 
      * @param key The configuration key
      * @return bool True if the key exists, false otherwise
      */
     bool has(const std::string& key) const;
 
     /**
      * @brief Get all configuration keys (flattened with dot notation)
      * 
      * @return std::vector<std::string> Vector of all configuration keys
      */
     std::vector<std::string> get_keys() const;
 
     /**
      * @brief Set a configuration value
      * 
      * @tparam T The type of the configuration value
      * @param key The configuration key
      * @param value The value to set
      */
     template <typename T>
     void set(const std::string& key, const T& value) {
         set_value(key, std::make_any<T>(value));
     }
 
     /**
      * @brief Merge another configuration into this one
      * 
      * @param other The other configuration to merge
      * @param override_existing Whether to override existing values (default: true)
      */
     void merge(const std::shared_ptr<Configuration>& other, bool override_existing = true);
 
     /**
      * @brief Load environment variables into the configuration
      * 
      * Environment variables prefixed with AF_ will be loaded into the configuration,
      * with the prefix removed and the rest of the variable name converted to lowercase
      * and dots. For example, AF_DATABASE_HOST will be loaded as database.host.
      * 
      * @param prefix The prefix for environment variables (default: "AF_")
      * @param override_existing Whether to override existing values (default: true)
      */
     void load_environment_variables(const std::string& prefix = "AF_", bool override_existing = true);
 
     /**
      * @brief Parse command-line arguments into the configuration
      * 
      * Command-line arguments in the format --key=value or --key value will be parsed
      * into the configuration. Keys can use dot notation for nested values.
      * 
      * @param argc Argument count from main()
      * @param argv Argument values from main()
      * @param override_existing Whether to override existing values (default: true)
      */
     void parse_command_line(int argc, char** argv, bool override_existing = true);
 
     /**
      * @brief Save the configuration to a YAML file
      * 
      * @param file_path Path to the output YAML file
      * @return bool True if the save was successful, false otherwise
      */
     bool save(const std::string& file_path) const;
 
 private:
     /**
      * @brief Constructor (private, use load() static method)
      */
     Configuration();
 
     // Internal representation of the configuration
     struct ConfigNode {
         std::unordered_map<std::string, std::any> values;
         std::unordered_map<std::string, std::shared_ptr<ConfigNode>> children;
     };
     std::shared_ptr<ConfigNode> root_;
 
     // Helper methods
     std::optional<std::any> get_value(const std::string& key) const;
     void set_value(const std::string& key, const std::any& value);
     std::vector<std::string> split_key(const std::string& key) const;
     
     std::string convert_to_string(const std::any& value) const;
     
     template <typename T>
     T convert_value(const std::any& value, const T& default_value) const {
         // Try to convert between common types
         try {
             if (value.type() == typeid(int)) {
                 if constexpr (std::is_floating_point_v<T>) {
                     return static_cast<T>(std::any_cast<int>(value));
                 }
             } else if (value.type() == typeid(double)) {
                 if constexpr (std::is_integral_v<T>) {
                     return static_cast<T>(std::any_cast<double>(value));
                 }
             } else if (value.type() == typeid(std::string)) {
                 // Convert string to numeric types
                 const std::string& str = std::any_cast<std::string>(value);
                 if constexpr (std::is_integral_v<T>) {
                     try {
                         return static_cast<T>(std::stoll(str));
                     } catch (...) {
                         return default_value;
                     }
                 } else if constexpr (std::is_floating_point_v<T>) {
                     try {
                         return static_cast<T>(std::stod(str));
                     } catch (...) {
                         return default_value;
                     }
                 } else if constexpr (std::is_same_v<T, bool>) {
                     return (str == "true" || str == "yes" || str == "1");
                 }
             } else if (value.type() == typeid(bool)) {
                 if constexpr (std::is_integral_v<T>) {
                     return std::any_cast<bool>(value) ? 1 : 0;
                 }
             }
         } catch (...) {
             // Conversion failed
         }
         
         return default_value;
     }
 
     // Load configuration from YAML file
     bool load_from_yaml(const std::string& file_path);
     
     // Helper for processing nested YAML nodes
     void process_yaml_node(const std::string& prefix, const void* node);
 };
 
 // Type alias for shared pointer to Configuration
 using ConfigurationPtr = std::shared_ptr<Configuration>;
 
 } // namespace config
 } // namespace af