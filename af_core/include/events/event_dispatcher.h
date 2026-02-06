#ifndef AF_CORE_EVENT_DISPATCHER_H
#define AF_CORE_EVENT_DISPATCHER_H

#include "i_event_dispatcher.h"
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <mutex>
#include <functional>

namespace af {
namespace core {
namespace events {

/**
 * @brief Concrete, thread-safe implementation of the IEventDispatcher interface.
 */
class EventDispatcher : public IEventDispatcher {
public:
    using SubscriberCallback = std::function<void(const BaseEvent&)>;

    /**
     * @brief Subscribes a listener to a specific event type.
     */
    template<typename TEvent>
    void subscribe(std::function<void(const TEvent&)> callback) {
        // Create a generic wrapper that safely casts the event back to the
        // specific type the subscriber is expecting.
        auto wrapper = [cb = std::move(callback)](const BaseEvent& event) {
            if (const auto* specific_event = dynamic_cast<const TEvent*>(&event)) {
                cb(*specific_event);
            }
        };

        std::lock_guard<std::mutex> lock(mtx_);
        subscribers_[std::type_index(typeid(TEvent))].push_back(wrapper);
    }

    /**
     * @brief Publishes an event to all interested subscribers.
     */
    void publish(const BaseEvent& event) override {
        auto event_type_index = std::type_index(typeid(event));
        
        std::vector<SubscriberCallback> callbacks_to_run;
        {
            // Lock only to get a copy of the callbacks. This minimizes lock time.
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = subscribers_.find(event_type_index);
            if (it != subscribers_.end()) {
                callbacks_to_run = it->second; // Copy the list
            }
        }

        // Run the callbacks outside of the lock to prevent deadlocks.
        for (const auto& callback : callbacks_to_run) {
            callback(event);
        }
    }

private:
    std::unordered_map<std::type_index, std::vector<SubscriberCallback>> subscribers_;
    std::mutex mtx_;
};

} // namespace events
} // namespace core
} // namespace af

#endif // AF_CORE_EVENT_DISPATCHER_H
