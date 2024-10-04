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

#include "ravennakit/core/platform.hpp"

#include <asio.hpp>
#include <iostream>

namespace rav {
class io_context_runner {
  public:
    io_context_runner() = default;

    /**
     * Constructs a runner with a specific number of threads.
     * @param num_threads Number of threads to run the io_context on.
     */
    explicit io_context_runner(const size_t num_threads) : num_threads_(num_threads) {}

    ~io_context_runner() {
        stop();
    }

    /**
     * Runs the runner until stop() is called, returning immediately.
     */
    void run() {
        stop();
        work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
            asio::make_work_guard(io_context_)
        );
        start();
    }

    /**
     * Runs all tasks to completion, returning immediately. Call stop to wait for all tasks to finish.
     */
    void run_to_completion() {
        stop();
        start();
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
        work_guard_.reset();
        io_context_.restart();
    }

    /**
     * @return The io_context used by this runner.
     */
    asio::io_context& io_context() {
        return io_context_;
    }

  private:
    const size_t num_threads_ = std::thread::hardware_concurrency();
    bool running_ = false;
    asio::io_context io_context_ {};
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
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
#if RAV_MACOS
                {
                    const std::string thread_name = "io_context_runner " + std::to_string(i);
                    pthread_setname_np(thread_name.c_str());
                }
#endif

                try {
                    io_context_.run();
                } catch (const std::exception& e) {
                    std::cerr << "Exception thrown on io_context runner thread: " << e.what() << "\n";
                } catch (...) {
                    std::cerr << "Unknown exception thrown on io_context runner thread\n";
                }
            });
        }

        running_ = true;
    }
};

}  // namespace rav
