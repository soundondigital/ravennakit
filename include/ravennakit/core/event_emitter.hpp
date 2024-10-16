/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include <functional>

namespace rav {

/**
 * A class that can emit events to listeners. Per event type one listener can be registered.
 * Inherit from this class, and call the `on` method to register a listener for a specific event type.
 * @tparam Subclass The type of the subclass.
 * @tparam Event List of supported event types.
 */
template<class Subclass, class... Event>
class event_emitter {
  public:
    template<class Type>
    using listener = std::function<void(Type&, Subclass&)>;

    virtual ~event_emitter() {
        static_assert(std::is_base_of_v<event_emitter, Subclass>);
    }

    /**
     * Registers a listener with the event emitter.
     * @tparam Type The type of the event.
     * @param f A valid listener to be registered.
     */
    template<class Type>
    void on(listener<Type> f) {
        handler<Type>() = std::move(f);
    }

    /**
     * Disconnects the listener for the given event type.
     * @tparam Type The type of the event.
     */
    template<class Type>
    void reset() noexcept {
        handler<Type>() = nullptr;
    }

    /**
     * Disconnects all listeners.
     */
    virtual void reset() noexcept {
        (reset<Event>(), ...);
    }

    /**
     * Checks if there is a listener registered for the specific event.
     * @tparam Type The type of the event.
     * @return True if a listener is registered, false otherwise.
     */
    template<class Type>
    [[nodiscard]] bool has_listener() const noexcept {
        return handler<Type>() != nullptr;
    }

protected:
    /**
     * Emits an event to the registered listener, calling the listener registered for the event type.
     * @tparam Type The type of the event.
     * @param event The event to publish.
     */
    template<class Type>
    void emit(Type event) {
        if (auto& l = handler<Type>(); l) {
            l(event, *static_cast<Subclass*>(this));
        }
    }

private:
    std::tuple<listener<Event>...> handlers{};

    template<class Type>
    const auto& handler() const noexcept {
        return std::get<listener<Type>>(handlers);
    }

    template<class Type>
    auto& handler() noexcept {
        return std::get<listener<Type>>(handlers);
    }
};

}  // namespace rav
