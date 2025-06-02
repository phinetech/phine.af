/**
 * @file service_provider.h
 * @brief Interface for service providers in the dependency injection framework
 * 
 * This file defines the ServiceProvider interface, which is the core abstraction
 * of the AF's dependency injection framework. Service providers are responsible for
 * resolving and providing service instances to components that depend on them.
 * 
 * The ServiceProvider interface includes:
 * - Type-safe methods to retrieve services by type
 * - Methods to retrieve named services (when multiple implementations of the same
 *   interface are available)
 * 
 * This interface is implemented by the ServiceContainer class, but the separation
 * allows components to depend only on the provider interface rather than the full
 * container implementation.
 * 
 * Usage:
 *   // From within a component that receives a ServiceProvider
 *   auto logger = provider.get<ILogger>();
 *   auto httpClient = provider.get<IHttpClient>("secure");
 */

 #pragma once

 #include <memory>
 #include <string>
 #include <typeindex>
 
 namespace af {
 namespace di {
 
 /**
  * @brief Interface for service providers
  * 
  * Service providers are responsible for creating and managing service instances
  */
 class ServiceProvider {
 public:
     virtual ~ServiceProvider() = default;
 
     /**
      * @brief Get service by type
      * 
      * @tparam T The service type to retrieve
      * @return std::shared_ptr<T> The service instance
      */
     template<typename T>
     std::shared_ptr<T> get() {
         return std::static_pointer_cast<T>(get_service(typeid(T)));
     }
 
     /**
      * @brief Get service by name and type
      * 
      * @tparam T The service type to retrieve
      * @param name The name of the service
      * @return std::shared_ptr<T> The service instance
      */
     template<typename T>
     std::shared_ptr<T> get(const std::string& name) {
         return std::static_pointer_cast<T>(get_named_service(typeid(T), name));
     }
 
 protected:
     /**
      * @brief Get service by type (internal implementation)
      * 
      * @param type The type information of the service
      * @return std::shared_ptr<void> The service instance as void pointer
      */
     virtual std::shared_ptr<void> get_service(const std::type_index& type) = 0;
 
     /**
      * @brief Get service by name and type (internal implementation)
      * 
      * @param type The type information of the service
      * @param name The name of the service
      * @return std::shared_ptr<void> The service instance as void pointer
      */
     virtual std::shared_ptr<void> get_named_service(const std::type_index& type, const std::string& name) = 0;
 };
 
 } // namespace di
 } // namespace af