/**
 * @file qod_handler.h
 * @brief Maps incoming CAMARA API data models to the application's internal domain models
 *
 * This class provides methods to convert between JSON representations used in the CAMARA
 * QualityOnDemand API and the internal C++ data structures representing QoD sessions,
 * devices, application servers, and related entities.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "../../common/models/qod/qod_session.h"
#include "../../common/models/qod/qod_events.h"
#include "../../common/communication/include/message.h"

namespace af {
namespace core {
    class AfOrchestrator; // Forward declaration

}
namespace qod {

/**
 * Notes on the methods to be implemented:
 * - The QoS Mapping methods should handle conversion between string profiles and
 *   internal enumerations or structures.
 * - qos_profiles can be creared via api so the mapping needs to happen here
 * - The PDU session to be mapped should be include in the internal session model.
 * - translate_device_to_ue_id
 * - store_session_mapping
 * - initialize_default_mappings
 * 
 */


/**
 * @brief Maps CAMARA API models to internal domain models and vice versa
 */
class QodApiMapper {
public:
    /**
     * @brief Constructor
     */
    QodApiMapper();

    /**
     * @brief Destructor
     */
    ~QodApiMapper();

    /**
     * @brief Initialize the API mapper
     * @param orchestrator Pointer to the orchestrator
     */
    void initialize(af::core::AfOrchestrator* orchestrator);
};

} // namespace qod
} // namespace af


