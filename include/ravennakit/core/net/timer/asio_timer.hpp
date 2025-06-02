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

#include "ravennakit/core/assert.hpp"

#include <boost/asio.hpp>

namespace rav {

/**
 * A timer that uses boost::asio::steady_timer to provide a simple way of creating a timer without thinking about the
 * lifetimes.
 */
class AsioTimer {
  public:
    using TimerCallback = std::function<void()>;

    explicit AsioTimer(boost::asio::io_context& io_context);
    ~AsioTimer();

    /**
     * Fires the callback once after the given duration.
     * The timer will be stopped before starting a new one.
     * @param duration The duration to wait before firing the callback.
     * @param cb The callback to fire after the duration.
     */
    void once(std::chrono::milliseconds duration, TimerCallback cb);

    /**
     * Fires the callback after the given duration. The callback will be fired repeatedly until stopped.
     * The timer will be stopped before starting a new one.
     * Safe to call from any thread.
     * @param duration The duration to wait before firing the callback.
     * @param cb The callback to fire after the duration.
     * @param repeating If true, the timer will repeat the callback after each duration.
     */
    void start(std::chrono::milliseconds duration, TimerCallback cb, bool repeating = true);

    /**
     * Stops the timer and cancels any pending callbacks. After this call returns, no more callbacks will be fired.
     * Can be called from any thread. The work is scheduled on the io_context thread.
     */
    void stop();

  private:
    boost::asio::steady_timer timer_;
    std::recursive_mutex mutex_;
    TimerCallback callback_;
    bool repeating_ = false;
    std::chrono::milliseconds duration_ = std::chrono::milliseconds(0);

    void wait();
};

}  // namespace rav
