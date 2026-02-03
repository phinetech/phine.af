/**
 * @file qod_error_codes.h
 * @brief QualityOnDemand service-specific error codes
 *
 * @details Error codes specific to the CAMARA QualityOnDemand API.
 * These extend the common CAMARA error codes with QoD-specific errors.
 *
 * For common errors (authentication, device identification, rate limiting, etc.),
 * use the codes from camara_error_codes.h instead.
 *
 * @see https://github.com/camaraproject/QualityOnDemand
 * @see https://github.com/camaraproject/Commonalities/blob/main/documentation/CAMARA-API-Design-Guide.md
 */

#pragma once

#include "common/models/camara/camara_error_codes.h"

namespace af {
namespace qod {

/**
 * @brief QualityOnDemand service-specific error codes
 *
 * @details These error codes are specific to the QualityOnDemand API and follow
 * the naming convention: QUALITY_ON_DEMAND.<SPECIFIC_CODE> as defined in
 * documentation/CAMARA-API-Design-Guide.md
 *
 * @note Use CommonErrorCode from camara_error_codes.h for generic errors such as:
 * - INVALID_ARGUMENT, OUT_OF_RANGE (input validation)
 * - UNAUTHENTICATED, PERMISSION_DENIED (authentication/authorization)
 * - NOT_FOUND, IDENTIFIER_NOT_FOUND (resource access)
 * - MISSING_IDENTIFIER, UNSUPPORTED_IDENTIFIER, UNNECESSARY_IDENTIFIER (device identification)
 * - SERVICE_NOT_APPLICABLE (service applicability)
 * - QUOTA_EXCEEDED, TOO_MANY_REQUESTS (rate limiting)
 * - INTERNAL, UNAVAILABLE, TIMEOUT (server errors)
 */
namespace QodErrorCode {

    /**
     * @brief Duration value is out of the allowed range
     * @http_status 400
     * @details The requested session duration exceeds the maximum allowed value
     * or is below the minimum allowed value for the specified QoS profile.
     * Implementations should document their specific duration limits.
     */
    constexpr const char* DURATION_OUT_OF_RANGE =
        "QUALITY_ON_DEMAND.DURATION_OUT_OF_RANGE";

    /**
     * @brief Session extension is not allowed
     * @http_status 409
     * @details The QoD session cannot be extended. This may occur when:
     * - The session has already expired
     * - The session is in a terminal state (e.g., TERMINATED)
     * - The QoS profile does not support session extension
     * - Business rules prevent extension for this session
     */
    constexpr const char* SESSION_EXTENSION_NOT_ALLOWED =
        "QUALITY_ON_DEMAND.SESSION_EXTENSION_NOT_ALLOWED";

    /**
     * @brief The requested QoS profile is not applicable for this device/network
     * @http_status 422
     * @details The requested QoS profile cannot be applied in the current context.
     * This may occur when:
     * - The device/UE does not support the requested QoS profile
     * - The network slice/DNN does not support the QoS profile
     * - The device is roaming and the profile is not available
     * - The device's subscription does not include this QoS profile
     */
    constexpr const char* QOS_PROFILE_NOT_APPLICABLE =
        "QUALITY_ON_DEMAND.QOS_PROFILE_NOT_APPLICABLE";

    /**
     * @brief The requested QoS profile does not exist
     * @http_status 404
     * @details The QoS profile name provided in the request is not recognized
     * by the implementation. Check API documentation for supported profiles.
     */
    constexpr const char* QOS_PROFILE_NOT_FOUND =
        "QUALITY_ON_DEMAND.QOS_PROFILE_NOT_FOUND";

    /**
     * @brief The requested QoS profile is not supported by this implementation
     * @http_status 422
     * @details The QoS profile exists in the CAMARA specification but is not
     * supported by this API provider's implementation.
     */
    constexpr const char* UNSUPPORTED_QOS_PROFILE =
        "QUALITY_ON_DEMAND.UNSUPPORTED_QOS_PROFILE";

    /**
     * @brief Bandwidth value is out of the allowed range
     * @http_status 400
     * @details The requested upstream/downstream bandwidth exceeds the maximum
     * allowed value or is below the minimum for the QoS profile.
     */
    constexpr const char* BANDWIDTH_OUT_OF_RANGE =
        "QUALITY_ON_DEMAND.BANDWIDTH_OUT_OF_RANGE";

    /**
     * @brief Maximum number of concurrent sessions exceeded
     * @http_status 409
     * @details The device or API consumer has reached the maximum number of
     * concurrent QoD sessions allowed. Delete existing sessions before creating new ones.
     */
    constexpr const char* SESSION_LIMIT_EXCEEDED =
        "QUALITY_ON_DEMAND.SESSION_LIMIT_EXCEEDED";
}

/**
 * @brief Convenience namespace that imports both common and QoD-specific codes
 *
 * @details This namespace re-exports both common CAMARA error codes and
 * QoD-specific error codes for convenience. API implementations can use:
 *
 * @example
 * ```cpp
 * using namespace af::qod::ErrorCode;
 *
 * // Use common CAMARA error
 * return create_error(400, INVALID_ARGUMENT, "Missing required field 'duration'");
 *
 * // Use QoD-specific error
 * return create_error(400, DURATION_OUT_OF_RANGE, "Duration must be 1-86400 seconds");
 * ```
 */
namespace ErrorCode {
    // Re-export common CAMARA error codes
    using namespace af::camara::CommonErrorCode;

    // Re-export QoD-specific error codes
    using namespace QodErrorCode;
}

} // namespace qod
} // namespace af
