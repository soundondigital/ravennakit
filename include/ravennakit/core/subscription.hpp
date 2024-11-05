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

#include "ravennakit/core/assert.hpp"

#include <functional>

namespace rav {

/**
 * A simple class which executes given function upon destruction.
 * Very suitable for subscriptions which must go out of scope when the owning class gets destructed.
 */
class subscription {
  public:
    subscription() = default;

    explicit subscription(std::function<void()> on_destruction_callback) :
        on_destruction_callback_(std::move(on_destruction_callback)) {}

    subscription(const subscription& other) = delete;
    subscription& operator=(const subscription& other) = delete;

    subscription(subscription&& other) noexcept {
        *this = std::move(other);
    }

    subscription& operator=(subscription&& other) noexcept {
        if (on_destruction_callback_) {
            on_destruction_callback_();
        }

        on_destruction_callback_ = std::move(other.on_destruction_callback_);
        other.on_destruction_callback_ = nullptr;

        RAV_ASSERT(other.on_destruction_callback_ == nullptr, "Move failed to reset the std::function");

        return *this;
    }

    ~subscription() {
        if (on_destruction_callback_) {
            on_destruction_callback_();
        }
    }

    /**
     * Assigns a new subscription callback. If there was a previous callback, it will be called.
     * @param on_destruction_callback The callback to invoke upon destruction.
     * @return The subscription itself.
     */
    subscription& operator=(std::function<void()>&& on_destruction_callback) noexcept {
        if (on_destruction_callback_) {
            on_destruction_callback_();
        }

        on_destruction_callback_ = std::move(on_destruction_callback);

        return *this;
    }

    /**
     * Returns true if the subscription is active.
     */
    explicit operator bool() const {
        return on_destruction_callback_ != nullptr;
    }

    /**
     * Resets this subscription by calling the callback (if any) and then clearing it.
     */
    void reset() {
        if (on_destruction_callback_) {
            on_destruction_callback_();
            on_destruction_callback_ = nullptr;
        }
    }

    /**
     * Releases (clears) the destruction callback without invoking it.
     * Warning! This basically negates the point of this class, you should probably rarely use it, if ever.
     */
    void release() {
        on_destruction_callback_ = nullptr;
    }

  private:
    std::function<void()> on_destruction_callback_;
};

/**
 * Convenience alias for subscription. Use this if you want to defer some action until the end of the scope.
 */
using defer = subscription;

}  // namespace rav
