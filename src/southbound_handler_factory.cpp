#include "southbound_handler_factory.h"

#include <iostream>
#include <vector>

#include "af_component.h"
#include "af_typed_config.hpp"

#ifdef AF_ENABLE_PCF_HANDLER
#include "pcf_handler.h"
#endif

namespace {

using HandlerFactory = std::unique_ptr<af::common::AfComponent> (*)(const std::string&);
using HandlerEnabledCheck = bool (*)(const af::config::AppConfig&);

struct SouthboundHandlerDescriptor {
    const char* display_name;
    HandlerEnabledCheck is_enabled;
    HandlerFactory create;
};

#ifdef AF_ENABLE_PCF_HANDLER
bool is_pcf_handler_enabled(const af::config::AppConfig& app_config) {
    return app_config.pcf_handler.enabled;
}

std::unique_ptr<af::common::AfComponent> create_pcf_handler(const std::string& config_path) {
    return std::make_unique<af::southbound::PcfHandler>(config_path);
}
#endif

#ifdef AF_ENABLE_NEF_HANDLER
bool is_nef_handler_enabled(const af::config::AppConfig& app_config) {
    return app_config.nef_handler.enabled;
}
#endif

#ifdef AF_ENABLE_UDR_HANDLER
bool is_udr_handler_enabled(const af::config::AppConfig& app_config) {
    return app_config.udr_handler.enabled;
}
#endif

std::vector<SouthboundHandlerDescriptor> make_handler_registry() {
    std::vector<SouthboundHandlerDescriptor> registry;

#ifdef AF_ENABLE_PCF_HANDLER
    registry.push_back({"pcf_handler", is_pcf_handler_enabled, create_pcf_handler});
#endif

#ifdef AF_ENABLE_NEF_HANDLER
    // Register nef_handler descriptor here when the implementation is available.
    (void)is_nef_handler_enabled;
#endif

#ifdef AF_ENABLE_UDR_HANDLER
    // Register udr_handler descriptor here when the implementation is available.
    (void)is_udr_handler_enabled;
#endif

    return registry;
}

} // namespace

namespace af::app {

SouthboundHandlerList create_southbound_handlers(const std::string& config_path) {
    SouthboundHandlerList handlers;
    const auto app_config = af::config::load_app_config(config_path);

    for (const auto& descriptor : make_handler_registry()) {
        if (descriptor.is_enabled(app_config)) {
            handlers.push_back(descriptor.create(config_path));
        } else {
            std::cout << "  - Skipping " << descriptor.display_name << " (disabled in config)." << std::endl;
        }
    }

    return handlers;
}

} // namespace af::app
