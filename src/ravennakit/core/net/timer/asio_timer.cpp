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

class rav::AsioTimer::Impl: public std::enable_shared_from_this<Impl> {
  public:
    explicit Impl(boost::asio::io_context& io_context) : timer_(io_context) {}

    void start(const std::chrono::milliseconds duration, Callback callback, const bool repeating) {
        RAV_ASSERT(callback != nullptr, "Callback must be valid");
        std::lock_guard lock(mutex_);
        callback_ = std::move(callback);
        repeating_ = repeating;
        async_wait(duration);
    }

    void stop() {
        std::lock_guard lock(mutex_);
        timer_.cancel();
        callback_ = nullptr;
        repeating_ = false;
    }

  private:
    boost::asio::steady_timer timer_;
    Callback callback_;
    bool repeating_ = false;
    std::recursive_mutex mutex_;

    void async_wait(const std::chrono::milliseconds duration) {
        timer_.expires_after(duration);
        timer_.async_wait([self = shared_from_this(), duration](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }
            if (ec) {
                RAV_ERROR("Timer error: {}", ec.message());
                return;
            }

            std::lock_guard lock(self->mutex_);

            if (self->callback_ == nullptr) {
                return;
            }

            self->callback_(ec);

            if (self->repeating_) {
                self->async_wait(duration);
            } else {
                self->callback_ = nullptr;
            }
        });
    }
};

rav::AsioTimer::AsioTimer(boost::asio::io_context& io_context) : impl_(std::make_shared<Impl>(io_context)) {}

rav::AsioTimer::~AsioTimer() {
    stop();
}

rav::AsioTimer::AsioTimer(AsioTimer&& other) noexcept : impl_(std::move(other.impl_)) {}

rav::AsioTimer& rav::AsioTimer::operator=(AsioTimer&& other) noexcept {
    if (this != &other) {
        if (impl_) {
            impl_->stop();  // Stop existing timer before overwriting
        }
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void rav::AsioTimer::once(const std::chrono::milliseconds duration, Callback callback) const {
    stop();
    impl_->start(duration, std::move(callback), false);
}

void rav::AsioTimer::start(const std::chrono::milliseconds duration, Callback callback) const {
    stop();
    impl_->start(duration, std::move(callback), true);
}

void rav::AsioTimer::stop() const {
    impl_->stop();
}
