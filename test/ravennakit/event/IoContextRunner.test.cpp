/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/event/IoContextRunner.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/util/chrono/Timeout.hpp"

#include <thread>

TEST_CASE("IoContextRunner :: run_to_completion()", "[IoContextRunner]") {
    SECTION("Run tasks to completion") {
        IoContextRunner runner;
        size_t expected_total = 0;
        std::atomic<size_t> total = 0;

        for (size_t i = 0; i < 10000; i++) {
            expected_total += i;
            asio::post(runner.io_context(), [&total, i] {
                total.fetch_add(i);
            });
        }

        runner.run_to_completion();

        REQUIRE(expected_total == total);
    }

    SECTION("Run tasks to completion a 2nd time") {
        IoContextRunner runner;
        size_t expected_total = 0;
        std::atomic<size_t> total = 0;

        for (size_t i = 0; i < 10000; i++) {
            expected_total += i;
            asio::post(runner.io_context(), [&total, i] {
                total.fetch_add(i);
            });
        }

        runner.run_to_completion();

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

        REQUIRE(expected_total == total);
    }

    SECTION("Scheduling task after run_to_completion() will not execute them") {
        IoContextRunner runner;
        std::atomic<size_t> total = 0;

        runner.run_to_completion();

        for (size_t i = 0; i < 10000; i++) {
            asio::post(runner.io_context(), [&total, i] {
                total.fetch_add(i);
            });
        }

        REQUIRE(total == 0);
    }
}

TEST_CASE("IoContextRunner :: run_to_completion_async()", "[IoContextRunner]") {
    SECTION("Run tasks to completion asynchronously") {
        IoContextRunner runner;
        size_t expected_total = 0;
        std::atomic<size_t> total = 0;

        for (size_t i = 0; i < 10000; i++) {
            expected_total += i;
            asio::post(runner.io_context(), [&total, i] {
                total.fetch_add(i);
            });
        }

        runner.run_to_completion_async();

        const rav::util::chrono::Timeout timeout(std::chrono::seconds(1));
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

    SECTION("Run tasks to completion asynchronously a 2nd time") {
        IoContextRunner runner;
        size_t expected_total = 0;
        std::atomic<size_t> total = 0;

        for (size_t i = 0; i < 10000; i++) {
            expected_total += i;
            asio::post(runner.io_context(), [&total, i] {
                total.fetch_add(i);
            });
        }

        runner.run_to_completion_async();

        const rav::util::chrono::Timeout timeout(std::chrono::seconds(1));
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

        runner.run_to_completion_async();

        const rav::util::chrono::Timeout timeout2(std::chrono::seconds(1));
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

TEST_CASE("IoContextRunner :: run()", "[IoContextRunner]") {
    SECTION("When calling run, the io_context should not stop when no work is posted") {
        IoContextRunner runner;
        std::atomic post_run_called = false;

        std::thread runner_thread([&runner] {
            runner.run();
        });

        // Give io_context some time to idle
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        REQUIRE(runner_thread.joinable());

        asio::post(runner.io_context(), [&post_run_called] {
            post_run_called = true;
        });

        const rav::util::chrono::Timeout timeout(std::chrono::seconds(1));
        while (!post_run_called) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (timeout.expired()) {
                FAIL("Timeout expired");
                break;
            }
        }

        runner.stop();
        runner_thread.join();
    }
}

TEST_CASE("IoContextRunner :: run_async()", "[IoContextRunner]") {
    SECTION("When calling run_async, the io_context should not stop when no work is posted") {
        IoContextRunner runner;
        std::atomic post_run_called = false;

        runner.run_async();

        // Give io_context some time to idle
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        asio::post(runner.io_context(), [&post_run_called] {
            post_run_called = true;
        });

        const rav::util::chrono::Timeout timeout(std::chrono::seconds(1));
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
