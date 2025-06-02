/**
 * @file af_config.cpp
 * @brief Implementation of the Configuration class
 * 
 * This file implements the Configuration class methods for loading, accessing,
 * and modifying configuration parameters. It supports loading from YAML files,
 * environment variables, and command-line arguments.
 */

 #include "af_config.h"
 #include <yaml-cpp/yaml.h>
 #include <fstream>
 #include <iostream>
 #include <cstdlib>
 #include <algorithm>
 #include <sstream>
 
#include <unistd.h>
extern char **environ;  // Explicitly declare the environ variable


 namespace af {
 namespace config {
 
 Configuration::Configuration() : root_(std::make_shared<ConfigNode>()) {
 }
 
 Configuration::~Configuration() {
 }
 
 std::shared_ptr<Configuration> Configuration::load(const std::string& file_path) {
     auto config = std::shared_ptr<Configuration>(new Configuration());
     if (!config->load_from_yaml(file_path)) {
         std::cerr << "Warning: Failed to load configuration from " << file_path << std::endl;
     }
     
     // Load environment variables
     config->load_environment_variables();
     
     return config;
 }
 
 std::shared_ptr<Configuration> Configuration::load(
     const std::string& file_path, int argc, char** argv) {
     auto config = load(file_path);
     
     // Parse command-line arguments
     config->parse_command_line(argc, argv);
     
     return config;
 }
 
 bool Configuration::has(const std::string& key) const {
     return get_value(key).has_value();
 }
 
 std::vector<std::string> Configuration::get_keys() const {
     std::vector<std::string> keys;
     
     std::function<void(const std::shared_ptr<ConfigNode>&, const std::string&)> collect_keys;
     collect_keys = [&](const std::shared_ptr<ConfigNode>& node, const std::string& prefix) {
         // Add values at this level
         for (const auto& pair : node->values) {
             keys.push_back(prefix + pair.first);
         }
         
         // Recursively add child keys
         for (const auto& pair : node->children) {
             std::string child_prefix = prefix + pair.first + ".";
             collect_keys(pair.second, child_prefix);
         }
     };
     
     collect_keys(root_, "");
     
     return keys;
 }
 
 void Configuration::merge(
     const std::shared_ptr<Configuration>& other, bool override_existing) {
     // Get all keys from the other configuration
     std::vector<std::string> other_keys = other->get_keys();
     
     // Merge each key
     for (const auto& key : other_keys) {
         if (override_existing || !has(key)) {
             auto value = other->get_value(key);
             if (value.has_value()) {
                 set_value(key, value.value());
             }
         }
     }
 }
 
 void Configuration::load_environment_variables(
     const std::string& prefix, bool override_existing) {
     // Get all environment variables
     char** env = environ;
     
     while (*env) {
         std::string env_var = *env;
         size_t equals_pos = env_var.find('=');
         
         if (equals_pos != std::string::npos) {
             std::string key = env_var.substr(0, equals_pos);
             std::string value = env_var.substr(equals_pos + 1);
             
             // Check if the key has the expected prefix
             if (key.substr(0, prefix.size()) == prefix) {
                 // Remove prefix and convert to lowercase with dots
                 std::string config_key = key.substr(prefix.size());
                 std::transform(config_key.begin(), config_key.end(), config_key.begin(),
                                [](unsigned char c) { return std::tolower(c); });
                 
                 // Replace underscores with dots
                 std::replace(config_key.begin(), config_key.end(), '_', '.');
                 
                 // Set the value if it doesn't exist or if override is enabled
                 if (override_existing || !has(config_key)) {
                     set_value(config_key, std::make_any<std::string>(value));
                 }
             }
         }
         
         env++;
     }
 }
 
 void Configuration::parse_command_line(
     int argc, char** argv, bool override_existing) {
     for (int i = 1; i < argc; i++) {
         std::string arg = argv[i];
         
         // Check if the argument is a key=value pair
         if (arg.substr(0, 2) == "--") {
             std::string key_value = arg.substr(2);
             size_t equals_pos = key_value.find('=');
             
             if (equals_pos != std::string::npos) {
                 // Format: --key=value
                 std::string key = key_value.substr(0, equals_pos);
                 std::string value = key_value.substr(equals_pos + 1);
                 
                 // Set the value if it doesn't exist or if override is enabled
                 if (override_existing || !has(key)) {
                     set_value(key, std::make_any<std::string>(value));
                 }
             } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                 // Format: --key value
                 std::string key = key_value;
                 std::string value = argv[i + 1];
                 
                 // Set the value if it doesn't exist or if override is enabled
                 if (override_existing || !has(key)) {
                     set_value(key, std::make_any<std::string>(value));
                 }
                 
                 // Skip the next argument (the value)
                 i++;
             } else {
                 // Format: --flag (boolean flag)
                 std::string key = key_value;
                 
                 // Set the value if it doesn't exist or if override is enabled
                 if (override_existing || !has(key)) {
                     set_value(key, std::make_any<bool>(true));
                 }
             }
         }
     }
 }
 
 bool Configuration::save(const std::string& file_path) const {
     YAML::Node root;
     
     // Function to recursively build YAML tree
     std::function<void(YAML::Node&, const std::shared_ptr<ConfigNode>&)> build_yaml;
     build_yaml = [&](YAML::Node& yaml_node, const std::shared_ptr<ConfigNode>& config_node) {
         // Add values at this level
         for (const auto& pair : config_node->values) {
             if (pair.second.type() == typeid(int)) {
                 yaml_node[pair.first] = std::any_cast<int>(pair.second);
             } else if (pair.second.type() == typeid(double)) {
                 yaml_node[pair.first] = std::any_cast<double>(pair.second);
             } else if (pair.second.type() == typeid(bool)) {
                 yaml_node[pair.first] = std::any_cast<bool>(pair.second);
             } else if (pair.second.type() == typeid(std::string)) {
                 yaml_node[pair.first] = std::any_cast<std::string>(pair.second);
             } else {
                 // Convert to string as fallback
                 yaml_node[pair.first] = convert_to_string(pair.second);
             }
         }
         
         // Recursively add child nodes
         for (const auto& pair : config_node->children) {
             YAML::Node child_node;
             build_yaml(child_node, pair.second);
             yaml_node[pair.first] = child_node;
         }
     };
     
     build_yaml(root, root_);
     
     try {
         std::ofstream fout(file_path);
         if (!fout.is_open()) {
             return false;
         }
         
         fout << YAML::Dump(root);
         return true;
     } catch (const std::exception& e) {
         std::cerr << "Error saving configuration: " << e.what() << std::endl;
         return false;
     }
 }
 
 std::optional<std::any> Configuration::get_value(const std::string& key) const {
     std::vector<std::string> parts = split_key(key);
     
     if (parts.empty()) {
         return std::nullopt;
     }
     
     std::shared_ptr<ConfigNode> current = root_;
     
     // Navigate to the correct node
     for (size_t i = 0; i < parts.size() - 1; i++) {
         const std::string& part = parts[i];
         auto it = current->children.find(part);
         
         if (it == current->children.end()) {
             return std::nullopt;
         }
         
         current = it->second;
     }
     
     // Get the value from the leaf node
     const std::string& leaf = parts.back();
     auto it = current->values.find(leaf);
     
     if (it == current->values.end()) {
         return std::nullopt;
     }
     
     return it->second;
 }
 
 void Configuration::set_value(const std::string& key, const std::any& value) {
     std::vector<std::string> parts = split_key(key);
     
     if (parts.empty()) {
         return;
     }
     
     std::shared_ptr<ConfigNode> current = root_;
     
     // Navigate/create the path to the correct node
     for (size_t i = 0; i < parts.size() - 1; i++) {
         const std::string& part = parts[i];
         auto it = current->children.find(part);
         
         if (it == current->children.end()) {
             // Create new node
             auto new_node = std::make_shared<ConfigNode>();
             current->children[part] = new_node;
             current = new_node;
         } else {
             current = it->second;
         }
     }
     
     // Set the value in the leaf node
     const std::string& leaf = parts.back();
     current->values[leaf] = value;
 }
 
 std::vector<std::string> Configuration::split_key(const std::string& key) const {
     std::vector<std::string> parts;
     std::stringstream ss(key);
     std::string part;
     
     while (std::getline(ss, part, '.')) {
         if (!part.empty()) {
             parts.push_back(part);
         }
     }
     
     return parts;
 }
 
 std::string Configuration::convert_to_string(const std::any& value) const {
     if (value.type() == typeid(std::string)) {
         return std::any_cast<std::string>(value);
     } else if (value.type() == typeid(int)) {
         return std::to_string(std::any_cast<int>(value));
     } else if (value.type() == typeid(double)) {
         return std::to_string(std::any_cast<double>(value));
     } else if (value.type() == typeid(bool)) {
         return std::any_cast<bool>(value) ? "true" : "false";
     } else {
         return ""; // Unsupported type
     }
 }
 
 bool Configuration::load_from_yaml(const std::string& file_path) {
     try {
         YAML::Node config = YAML::LoadFile(file_path);
         process_yaml_node("", &config);
         return true;
     } catch (const YAML::Exception& e) {
         std::cerr << "Error loading YAML configuration: " << e.what() << std::endl;
         return false;
     } catch (const std::exception& e) {
         std::cerr << "Error loading configuration: " << e.what() << std::endl;
         return false;
     }
 }
 
 void Configuration::process_yaml_node(const std::string& prefix, const void* node) {
     const YAML::Node& yaml_node = *static_cast<const YAML::Node*>(node);
     
     if (yaml_node.IsMap()) {
         // Process map (object) nodes
         for (const auto& pair : yaml_node) {
             std::string key = pair.first.as<std::string>();
             std::string full_key = prefix.empty() ? key : prefix + "." + key;
             
             if (pair.second.IsMap() || pair.second.IsSequence()) {
                 // Recursively process nested maps and sequences
                 process_yaml_node(full_key, &pair.second);
             } else {
                 // Set scalar values
                 if (pair.second.IsScalar()) {
                     // Try to determine the type
                     try {
                         // Try to parse as boolean
                         bool bool_val;
                         if (YAML::convert<bool>::decode(pair.second, bool_val)) {
                             set_value(full_key, std::make_any<bool>(bool_val));
                             continue;
                         }
                     } catch (...) {}
                     
                     try {
                         // Try to parse as integer
                         int int_val;
                         if (YAML::convert<int>::decode(pair.second, int_val)) {
                             set_value(full_key, std::make_any<int>(int_val));
                             continue;
                         }
                     } catch (...) {}
                     
                     try {
                         // Try to parse as double
                         double double_val;
                         if (YAML::convert<double>::decode(pair.second, double_val)) {
                             set_value(full_key, std::make_any<double>(double_val));
                             continue;
                         }
                     } catch (...) {}
                     
                     // Default to string
                     set_value(full_key, std::make_any<std::string>(pair.second.as<std::string>()));
                 }
             }
         }
     } else if (yaml_node.IsSequence()) {
         // Process sequence (array) nodes
         // For now, we don't support arrays in the configuration
         // If needed, this could be extended to support arrays
     }
 }
 
 } // namespace config
 } // namespace af