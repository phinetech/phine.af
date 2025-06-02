/**
 * @file service_container.h
 * @brief Service container implementation for the AF dependency injection framework
 * 
 * This file defines the ServiceContainer class, which is the central component of
 * the AF's dependency injection system. The container manages service registration,
 * resolution, and lifetime.
 * 
 * The ServiceContainer supports:
 * - Registration of service factories
 * - Registration of concrete instances
 * - Named services (multiple implementations of the same interface)
 * - Singleton services (created once and reused)
 * - Transient services (created each time they're requested)
 * 
 * This container implementation forms the backbone of the AF's modular architecture,
 * allowing components to depend on abstractions rather than concrete implementations
 * and facilitating testing through dependency substitution.
 * 
 * Usage:
 *   // Create a container
 *   ServiceContainer container;
 *   
 *   // Register a singleton service
 *   container.register_singleton<ILogger>([](ServiceProvider& p) {
 *     return std::make_shared<FileLogger>("app.log");
 *   });
 *   
 *   // Register a transient service
 *   container.register_factory<IHttpClient>([](ServiceProvider& p) {
 *     auto logger = p.get<ILogger>();
 *     return std::make_shared<HttpClient>(logger);
 *   });
 *   
 *   // Resolve a service
 *   auto logger = container.get<ILogger>();
 */

 #pragma once

 #include <memory>
 #include <unordered_map>
 #include <functional>
 #include <typeindex>
 #include <stdexcept>
 #include "service_provider.h"
 #include "service_factory.h"
 
 namespace af {
 namespace di {
 
 /**
  * @brief Service container implementation
  * 
  * This class is responsible for registering and resolving services
  */
 class ServiceContainer : public ServiceProvider {
 public:
     /**
      * @brief Register a service factory
      * 
      * @tparam T The service type
      * @param factory The service factory
      */
     template<typename T>
     void register_factory(std::shared_ptr<ServiceFactory<T>> factory) {
         factories_[typeid(T)] = [factory](ServiceProvider& provider) {
             return std::static_pointer_cast<void>(factory->create(provider));
         };
     }
 
     /**
      * @brief Register a service factory with a name
      * 
      * @tparam T The service type
      * @param name The service name
      * @param factory The service factory
      */
     template<typename T>
     void register_factory(const std::string& name, std::shared_ptr<ServiceFactory<T>> factory) {
         named_factories_[{typeid(T), name}] = [factory](ServiceProvider& provider) {
             return std::static_pointer_cast<void>(factory->create(provider));
         };
     }
 
     /**
      * @brief Register a service instance
      * 
      * @tparam T The service type
      * @param instance The service instance
      */
     template<typename T>
     void register_instance(std::shared_ptr<T> instance) {
         instances_[typeid(T)] = std::static_pointer_cast<void>(instance);
     }
 
     /**
      * @brief Register a service instance with a name
      * 
      * @tparam T The service type
      * @param name The service name
      * @param instance The service instance
      */
     template<typename T>
     void register_instance(const std::string& name, std::shared_ptr<T> instance) {
         named_instances_[{typeid(T), name}] = std::static_pointer_cast<void>(instance);
     }
 
     /**
      * @brief Register a service factory using a factory function
      * 
      * @tparam T The service type
      * @param function The factory function
      */
     template<typename T>
     void register_factory(std::function<std::shared_ptr<T>(ServiceProvider&)> function) {
         auto factory = std::make_shared<FunctionServiceFactory<T>>(std::move(function));
         register_factory<T>(factory);
     }
 
     /**
      * @brief Register a service factory with a name using a factory function
      * 
      * @tparam T The service type
      * @param name The service name
      * @param function The factory function
      */
     template<typename T>
     void register_factory(const std::string& name, std::function<std::shared_ptr<T>(ServiceProvider&)> function) {
         auto factory = std::make_shared<FunctionServiceFactory<T>>(std::move(function));
         register_factory<T>(name, factory);
     }
 
     /**
      * @brief Register a singleton service
      * 
      * @tparam T The service type
      * @param function The factory function
      */
     template<typename T>
     void register_singleton(std::function<std::shared_ptr<T>(ServiceProvider&)> function) {
         singletons_[typeid(T)] = [function, this](ServiceProvider& provider) {
             auto& instance = singleton_instances_[typeid(T)];
             if (!instance) {
                 instance = std::static_pointer_cast<void>(function(provider));
             }
             return instance;
         };
     }
 
     /**
      * @brief Register a named singleton service
      * 
      * @tparam T The service type
      * @param name The service name
      * @param function The factory function
      */
     template<typename T>
     void register_singleton(const std::string& name, std::function<std::shared_ptr<T>(ServiceProvider&)> function) {
         named_singletons_[{typeid(T), name}] = [function, this, name](ServiceProvider& provider) {
             auto key = std::make_pair(typeid(T), name);
             auto& instance = named_singleton_instances_[key];
             if (!instance) {
                 instance = std::static_pointer_cast<void>(function(provider));
             }
             return instance;
         };
     }
 
 protected:
     /**
      * @brief Get service by type
      * 
      * @param type The service type
      * @return std::shared_ptr<void> The service instance
      */
     std::shared_ptr<void> get_service(const std::type_index& type) override;
 
     /**
      * @brief Get service by name and type
      * 
      * @param type The service type
      * @param name The service name
      * @return std::shared_ptr<void> The service instance
      */
     std::shared_ptr<void> get_named_service(const std::type_index& type, const std::string& name) override;
 
 private:
     // Type aliases for better readability
     using FactoryFunction = std::function<std::shared_ptr<void>(ServiceProvider&)>;
     using NamedKey = std::pair<std::type_index, std::string>;
 
     // Hash function for NamedKey
     struct NamedKeyHash {
         std::size_t operator()(const NamedKey& key) const {
             return std::hash<std::type_index>()(key.first) ^ std::hash<std::string>()(key.second);
         }
     };
 
     // Maps for service factories and instances
     std::unordered_map<std::type_index, FactoryFunction> factories_;
     std::unordered_map<NamedKey, FactoryFunction, NamedKeyHash> named_factories_;
     std::unordered_map<std::type_index, std::shared_ptr<void>> instances_;
     std::unordered_map<NamedKey, std::shared_ptr<void>, NamedKeyHash> named_instances_;
     std::unordered_map<std::type_index, FactoryFunction> singletons_;
     std::unordered_map<NamedKey, FactoryFunction, NamedKeyHash> named_singletons_;
     std::unordered_map<std::type_index, std::shared_ptr<void>> singleton_instances_;
     std::unordered_map<NamedKey, std::shared_ptr<void>, NamedKeyHash> named_singleton_instances_;
 };
 
 } // namespace di
 } // namespace af