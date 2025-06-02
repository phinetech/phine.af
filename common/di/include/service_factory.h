/**
 * @file service_factory.h
 * @brief Factory interfaces for creating service instances in the DI framework
 * 
 * This file defines the ServiceFactory interface and implementations used in the
 * AF's dependency injection framework. Service factories are responsible for creating
 * instances of services when they're requested from the container.
 * 
 * The file includes:
 * - ServiceFactory<T>: Abstract factory interface for creating services of type T
 * - FunctionServiceFactory<T>: Concrete factory that uses a function to create services
 * 
 * These factories enable flexible service creation strategies, from simple lambdas to
 * complex factory implementations with custom logic.
 * 
 * Usage:
 *   // Register a service with a lambda factory
 *   container.register_factory<ILogger>([](ServiceProvider& p) {
 *     return std::make_shared<ConsoleLogger>();
 *   });
 * 
 *   // Or create a custom factory
 *   class DatabaseFactory : public ServiceFactory<IDatabase> {
 *     std::shared_ptr<IDatabase> create(ServiceProvider& p) override {
 *       auto config = p.get<IConfiguration>();
 *       return std::make_shared<PostgresDatabase>(config->get_connection_string());
 *     }
 *   };
 *   container.register_factory<IDatabase>(std::make_shared<DatabaseFactory>());
 */

 #pragma once

 #include <memory>
 #include "service_provider.h"
 
 namespace af {
 namespace di {
 
 /**
  * @brief Interface for service factories
  * 
  * Service factories are responsible for creating service instances
  * 
  * @tparam T The service type to create
  */
 template<typename T>
 class ServiceFactory {
 public:
     virtual ~ServiceFactory() = default;
 
     /**
      * @brief Create a service instance
      * 
      * @param provider The service provider for dependency resolution
      * @return std::shared_ptr<T> The created service instance
      */
     virtual std::shared_ptr<T> create(ServiceProvider& provider) = 0;
 };
 
 /**
  * @brief Factory implementation that creates services using a factory function
  * 
  * @tparam T The service type to create
  */
 template<typename T>
 class FunctionServiceFactory : public ServiceFactory<T> {
 public:
     using FactoryFunction = std::function<std::shared_ptr<T>(ServiceProvider&)>;
 
     /**
      * @brief Construct a new Function Service Factory
      * 
      * @param function The factory function
      */
     explicit FunctionServiceFactory(FactoryFunction function) 
         : function_(std::move(function)) {}
 
     /**
      * @brief Create a service instance using the factory function
      * 
      * @param provider The service provider for dependency resolution
      * @return std::shared_ptr<T> The created service instance
      */
     std::shared_ptr<T> create(ServiceProvider& provider) override {
         return function_(provider);
     }
 
 private:
     FactoryFunction function_;
 };
 
 } // namespace di
 } // namespace af