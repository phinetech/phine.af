#ifndef AF_CORE_I_EVENT_DISPATCHER_H
#define AF_CORE_I_EVENT_DISPATCHER_H

#include <functional>
#include <memory>
#include "events.h"

namespace af {
namespace core {
namespace events {

using SubscriberCallback = std::function<void(const BaseEvent&)>;

class IEventDispatcher {
public:
    virtual ~IEventDispatcher() = default;

    /**
     * @brief Subscribes a listener to a specific event type.
     */
    template<typename TEvent>
    virtual void subscribe(std::function<void(const TEvent&)> callback) = 0;

    /**
     * @brief Publishes an event to all interested subscribers.
     */
    virtual void publish(const BaseEvent& event) = 0;
};

} // namespace events
} // namespace core
} // namespace af

#endif // AF_CORE_I_EVENT_DISPATCHER_H