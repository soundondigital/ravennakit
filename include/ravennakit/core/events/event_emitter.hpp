/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/linked_node.hpp"

namespace rav {

/**
 * An alias for an event slot which can be used to subscribe to events.
 */
template<typename... Args>
using event_slot = linked_node<std::function<void(Args...)>>;

/**
 * An event emitter which can be used to subscribe to and emit events of a given type.
 * The way this class is designed makes it easy to use without thinking about memory management. When the event_slot
 * goes out of scope it will unsubscribe itself from the event emitter (by removing itself from the internal linked
 * list) and when the event_emitter goes out of scope while there are existing event_slots, it removes itself from the
 * list after which it cannot be dereferenced anymore. Another advantage of this class is having the ability to
 * subscribe to events without inheritance.
 * @tparam Args List of arguments to be emitted.
 */
template<typename... Args>
class event_emitter {
  public:
    using handle = linked_node<std::function<void(Args...)>>;

    /**
     * Emits an event to all subscribers.
     * @param args The arguments to pass to the subscribers.
     */
    void operator()(Args... args) {
        emit(args...);
    }

    /**
     * Subscribes to updates from the event emitter. The subscription will be automatically removed when the handle is
     * destroyed. A new function can be assigned to the handle to change the subscription.
     * @param f The function to be called when the event is emitted.
     * @return A handle to the subscription.
     */
    [[nodiscard]] event_slot<Args...> subscribe(std::function<void(Args...)> f) {
        handle node(std::move(f));
        subscribers_.push_back(node);
        return node;
    }

    /**
     * Emits an event to all subscribers.
     * @param args The arguments to pass to the subscribers.
     */
    void emit(Args... args) {
        for (auto& s : subscribers_) {
            auto& f = s.value();
            if (f == nullptr) {
                continue;
            }
            f(args...);
        }
    }

  private:
    linked_node<std::function<void(Args...)>> subscribers_;
};

}  // namespace rav
