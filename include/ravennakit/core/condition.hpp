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

#include <mutex>
#include <condition_variable>

namespace rav {

/**
 * Simple mechanism for signalling between threads. One thread can wait, while another can signal.
 * In most cases you'll likely want to use a std::promise and std::future pair instead.
 */
class condition {
  public:
    /**
     * Waits until signaled.
     */
    void wait() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] {
            return ready_;
        });
    }

    /**
     * Waits until ready, or when reaching timeout.
     * @param timeout_ms
     * @return True if the signal became ready, or false if the wait timed out.
     */
    bool wait_for_ms(const int timeout_ms) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
            return ready_;
        });
    }

    /**
     * Signal the other thread.
     */
    void signal() {
        std::lock_guard lock(mutex_);
        ready_ = true;
        cv_.notify_all();
    }

    /**
     * Reset.
     */
    void reset() {
        std::lock_guard lock(mutex_);
        ready_ = false;
    }

  private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool ready_ {false};
};

}  // namespace rav
