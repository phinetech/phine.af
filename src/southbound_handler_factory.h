#pragma once

#include <memory>
#include <string>
#include <vector>

namespace af::common {
class AfComponent;
}

namespace af::app {

using SouthboundHandlerList = std::vector<std::unique_ptr<af::common::AfComponent>>;

SouthboundHandlerList create_southbound_handlers(const std::string& config_path);

} // namespace af::app
