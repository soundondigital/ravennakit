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

#include <chrono>
#include <thread>

namespace rav::util::chrono {

/**
 * Simple timeout class which blocks the current thread until the timeout expires.
 */
template<class Rep, class Period>
class timeout {
  public:
    explicit timeout(const std::chrono::duration<Rep, Period>& duration) : duration_(duration) {}

    /**
     * @returns True if the timeout has expired.
     */
    [[nodiscard]] bool expired() const {
        return std::chrono::steady_clock::now() - start_point_ > duration_;
    }

    /**
     * Busy-waits (with 100ms sleeps) until condition becomes true or timeout expires.
     * @param condition The condition to wait for.
     * @return True if the condition became true before the timeout expired, or false if the timeout expired.
     */
    [[maybe_unused]] bool wait_until(const std::function<bool()>& condition) const {
        while (!condition) {
            if (expired()) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return true;
    }

  private:
    const std::chrono::time_point<std::chrono::steady_clock> start_point_ {std::chrono::steady_clock::now()};
    const std::chrono::duration<Rep, Period>& duration_;
};

}  // namespace rav::util::chrono
