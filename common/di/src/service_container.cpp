/**
 * @file service_container.cpp
 * @brief Implementation of the ServiceContainer class
 * 
 * This file provides the implementation of the ServiceContainer methods defined
 * in service_container.h. It handles the core logic for resolving services from
 * the container, including checking for registered instances, singletons, and factories.
 * 
 * The implementation prioritizes:
 * 1. Returning pre-registered instances
 * 2. Creating/retrieving singleton instances
 * 3. Creating new instances from factories
 * 
 * If a service cannot be resolved, an exception is thrown with details about
 * the missing service.
 */

 #include "service_container.h"
 #include <stdexcept>
 
 namespace af {
 namespace di {
 
 std::shared_ptr<void> ServiceContainer::get_service(const std::type_index& type) {
     // Check if instance exists
     auto instance_it = instances_.find(type);
     if (instance_it != instances_.end()) {
         return instance_it->second;
     }
 
     // Check if singleton exists
     auto singleton_it = singletons_.find(type);
     if (singleton_it != singletons_.end()) {
         return singleton_it->second(*this);
     }
 
     // Try to create from factory
     auto factory_it = factories_.find(type);
     if (factory_it != factories_.end()) {
         return factory_it->second(*this);
     }
 
     throw std::runtime_error("Service not registered for type: " + std::string(type.name()));
 }
 
 std::shared_ptr<void> ServiceContainer::get_named_service(const std::type_index& type, const std::string& name) {
     NamedKey key{type, name};
 
     // Check if named instance exists
     auto instance_it = named_instances_.find(key);
     if (instance_it != named_instances_.end()) {
         return instance_it->second;
     }
 
     // Check if named singleton exists
     auto singleton_it = named_singletons_.find(key);
     if (singleton_it != named_singletons_.end()) {
         return singleton_it->second(*this);
     }
 
     // Try to create from named factory
     auto factory_it = named_factories_.find(key);
     if (factory_it != named_factories_.end()) {
         return factory_it->second(*this);
     }
 
     throw std::runtime_error("Named service not registered: " + name + " for type: " + std::string(type.name()));
 }
 
 } // namespace di
 } // namespace af