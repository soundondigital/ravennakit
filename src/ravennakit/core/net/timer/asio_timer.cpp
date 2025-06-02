/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/timer/asio_timer.hpp"

#include "ravennakit/core/log.hpp"

rav::AsioTimer::AsioTimer(boost::asio::io_context& io_context) : timer_(io_context) {}

rav::AsioTimer::~AsioTimer() {
    stop();
}

void rav::AsioTimer::once(const std::chrono::milliseconds duration, TimerCallback cb) {
    start(duration, std::move(cb), false);
}

void rav::AsioTimer::start(const std::chrono::milliseconds duration, TimerCallback cb, const bool repeating) {
    std::lock_guard lock(mutex_);
    timer_.cancel();
    callback_ = std::move(cb);
    duration_ = duration;
    repeating_ = repeating;
    wait();
}

void rav::AsioTimer::stop() {
    std::lock_guard lock(mutex_);
    timer_.cancel();
    callback_ = nullptr;
    repeating_ = false;
}

void rav::AsioTimer::wait() {
    timer_.expires_after(duration_);
    timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        if (ec) {
            RAV_ERROR("Timer error: {}", ec.message());
            return;
        }

        std::lock_guard lock(mutex_);
        if (!callback_) {
            return;
        }
        callback_();
        if (repeating_) {
            wait();
            return;
        }
        callback_ = nullptr;
    });
}
