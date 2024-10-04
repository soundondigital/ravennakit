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

#include "bonjour.hpp"
#include "ravennakit/platform/posix/pipe.hpp"
#include "ravennakit/platform/windows/event.hpp"

#include <future>
#include <thread>

#if RAV_HAS_APPLE_DNSSD

namespace rav::dnssd {

/**
 * Class which processes the results of a DNSServiceRef in a separate thread.
 */
class process_results_thread {
  public:
    ~process_results_thread();

    /**
     * Starts the thread to process the results of a DNSServiceRef. Thread must not already be running.
     * @param service_ref The DNSServiceRef to process.
     */
    void start(DNSServiceRef service_ref);

    /**
     * Stops the thread. If the thread is not running, nothing happens.
     */
    void stop();

    /**
     * @return True if the thread is running, false otherwise.
     */
    bool is_running();

    /**
     * Locks part of the thread. Used for synchronization of callbacks and the main thread.
     * @return A lock_guard for the mutex.
     */
    std::lock_guard<std::mutex> lock();

  private:
    int service_fd_ {};
#if RAV_POSIX
    posix::pipe pipe_;
#elif RAV_WINDOWS
    windows::event event_;
#endif
    std::mutex lock_;
    std::future<void> future_;

    void run(DNSServiceRef service_ref, int service_fd);
};

}  // namespace rav::dnssd

#endif
