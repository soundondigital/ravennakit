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
 * Wrapper around std::tuple for holding different kinds of events.
 * @tparam Events The events to be held.
 */
template<class... Events>
class events {
  public:
    events() = default;
    virtual ~events() = default;

    events(const events&) = delete;
    events& operator=(const events&) = delete;

    events(events&&) = default;
    events& operator=(events&&) = default;

    template<class Type>
    using handler = std::function<void(const Type&)>;

    /**
     * Registers a handler for given event type.
     * @tparam Type The type of the event.
     * @param f A valid handler to be registered.
     */
    template<class Type>
    void on(handler<Type> f) {
        get<Type>() = std::move(f);
    }

    /**
     * Deletes the handler for the given event type.
     * @tparam Type The type of the event.
     */
    template<class Type>
    void reset() noexcept {
        get<Type>() = nullptr;
    }

    /**
     * Deletes all handlers.
     */
    virtual void reset() noexcept {
        (reset<Events>(), ...);
    }

    /**
     * Checks if there is a handler registered for the specific event.
     * @tparam Type The type of the event.
     * @return True if a handler is registered, false otherwise.
     */
    template<class Type>
    [[nodiscard]] bool has_handler() const noexcept {
        return get<Type>() != nullptr;
    }

    /**
     * Calls handler for given event type.
     * @tparam Type The type of the event.
     * @param event The event to publish.
     */
    template<class Type>
    void emit(const Type& event) {
        if (auto& h = get<Type>(); h) {
            h(event);
        }
    }

  private:
    std::tuple<handler<Events>...> handlers {};

    template<class Type>
    const auto& get() const noexcept {
        return std::get<handler<Type>>(handlers);
    }

    template<class Type>
    auto& get() noexcept {
        return std::get<handler<Type>>(handlers);
    }
};

}  // namespace rav
