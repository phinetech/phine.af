#ifndef AF_CORE_EVENTS_H
#define AF_CORE_EVENTS_H

#include <string>
#include <vector>
#include <any>
#include "common/models/common.h" // For Supi, etc.

namespace af {
namespace core {
namespace events {

struct BaseEvent {
    virtual ~BaseEvent() = default;
};

// Event published when a PDU session's state changes to RELEASED or is removed.
struct PduSessionTerminatedEvent : public BaseEvent {
    Supi supi;
    std::string pdu_session_id;

    /**
     * @brief Constructor to allow for direct initialization with arguments.
     * @param s The SUPI of the UE.
     * @param p_id The ID of the terminated PDU session.
     */
    PduSessionTerminatedEvent(const Supi& s, const std::string& p_id)
        : supi(s), pdu_session_id(p_id) {}
};

// Add other events here


} // namespace events
} // namespace core
} // namespace af

#endif // AF_CORE_EVENTS_H
