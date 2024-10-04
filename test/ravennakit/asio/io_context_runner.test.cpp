/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/asio/io_context_runner.hpp"
#include "ravennakit/core/log.hpp"

#include <catch2/catch_all.hpp>
#include <thread>

#include "ravennakit/util/chrono/timeout.hpp"

namespace {
constexpr auto k_default_timeout_seconds_seconds = std::chrono::seconds(5);
}

TEST_CASE("io_context_runner | run_to_completion_async()", "[io_context_runner]") {
    SECTION("Run tasks to completion asynchronously") {
        rav::io_context_runner runner;
        size_t expected_total = 0;
        std::atomic<size_t> total = 0;

        for (size_t i = 0; i < 10000; i++) {
            expected_total += i;
            asio::post(runner.io_context(), [&total, i] {
                total.fetch_add(i);
            });
        }

        runner.run_to_completion();

        const rav::util::chrono::timeout timeout(k_default_timeout_seconds_seconds);
        while (total != expected_total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (timeout.expired()) {
                FAIL("Timeout expired");
                break;
            }
        }

        runner.stop();

        REQUIRE(expected_total == total);
    }

    SECTION("Run tasks to completion asynchronously two times") {
        rav::io_context_runner runner;
        size_t expected_total = 0;
        std::atomic<size_t> total = 0;

        for (size_t i = 0; i < 10000; i++) {
            expected_total += i;
            asio::post(runner.io_context(), [&total, i] {
                total.fetch_add(i);
            });
        }

        runner.run_to_completion();

        const rav::util::chrono::timeout timeout(k_default_timeout_seconds_seconds);
        while (total != expected_total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (timeout.expired()) {
                FAIL("Timeout expired");
                break;
            }
        }

        runner.stop();

        REQUIRE(expected_total == total);

        expected_total = 0;
        total = 0;

        for (size_t i = 0; i < 10000; i++) {
            expected_total += i;
            asio::post(runner.io_context(), [&total, i] {
                total.fetch_add(i);
            });
        }

        runner.run_to_completion();

        const rav::util::chrono::timeout timeout2(k_default_timeout_seconds_seconds);
        while (total != expected_total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (timeout2.expired()) {
                FAIL("Timeout expired");
                break;
            }
        }

        runner.stop();

        REQUIRE(expected_total == total);
    }
}

TEST_CASE("io_context_runner | run_async()", "[io_context_runner]") {
    SECTION("When calling run_async, the io_context should not stop when no work is posted") {
        rav::io_context_runner runner;
        std::atomic post_run_called = false;

        runner.run();

        // Give io_context some time to idle
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        asio::post(runner.io_context(), [&post_run_called] {
            post_run_called = true;
        });

        const rav::util::chrono::timeout timeout(k_default_timeout_seconds_seconds);
        while (!post_run_called) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (timeout.expired()) {
                FAIL("Timeout expired");
                break;
            }
        }

        runner.stop();
    }
}
