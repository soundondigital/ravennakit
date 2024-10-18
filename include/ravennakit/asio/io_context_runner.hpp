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

#include "ravennakit/core/exception.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/platform.hpp"
#include "ravennakit/util/tracy.hpp"

#include <asio.hpp>

namespace rav {

/**
 * Helper class to run an asio::io_context on multiple threads.
 */
class io_context_runner {
  public:
    io_context_runner() {
        start();
    }

    /**
     * Constructs a runner with a specific number of threads.
     * @param num_threads Number of threads to run the io_context on.
     */
    explicit io_context_runner(const size_t num_threads) :
        num_threads_(num_threads), io_context_(static_cast<int>(num_threads)) {
        start();
    }

    ~io_context_runner() {
        stop();
    }

    /**
     * Stops the runner and waits for all threads to finish.
     */
    void stop() {
        io_context_.stop();
        for (auto& thread : threads_) {
            thread.join();
        }
        threads_.clear();
    }

    /**
     * Resets the work_guard and waits until the runner has no more work to do.
     */
    void join() {
        work_guard_.reset();
        for (auto& thread : threads_) {
            thread.join();
        }
        threads_.clear();
    }

    /**
     * @return True if the runner is currently running, false otherwise.
     */
    [[nodiscard]] bool is_running() const {
        return !threads_.empty();
    }

    /**
     * @return The io_context used by this runner.
     */
    asio::io_context& io_context() {
        return io_context_;
    }

  private:
    const size_t num_threads_ = std::thread::hardware_concurrency();
    asio::io_context io_context_ {};
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_ {asio::make_work_guard(io_context_)};
    std::vector<std::thread> threads_ {};

    /**
     * Starts the runner asynchronously, returning immediately. If the runner is already running, it will be stopped
     * first.
     */
    void start() {
        threads_.clear();
        threads_.reserve(num_threads_);

        for (size_t i = 0; i < num_threads_; i++) {
            threads_.emplace_back([this, i] {
                TRACY_ZONE_SCOPED;

#if RAV_MACOS
                {
                    const std::string thread_name = "io_context_runner " + std::to_string(i);
                    pthread_setname_np(thread_name.c_str());
                }
#endif

                while (true) {
                    try {
                        io_context_.run();
                        if (io_context_.stopped()) {
                            break;
                        }
                    } catch (const std::exception& e) {
                        RAV_ERROR("Exception thrown on io_context runner thread: {}", e.what());
                    } catch (...) {
                        RAV_ERROR("Unknown exception thrown on io_context runner thread");
                    }
                }
            });
        }
    }
};

}  // namespace rav
