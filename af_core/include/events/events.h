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
};

// Add other events here 


} // namespace events
} // namespace core
} // namespace af

#endif // AF_CORE_EVENTS_H
