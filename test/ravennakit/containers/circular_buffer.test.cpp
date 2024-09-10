/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <catch2/catch_all.hpp>
#include <ravennakit/containers/circular_buffer.hpp>
#include <ravennakit/containers/detail/fifo.hpp>
#include <ravennakit/core/log.hpp>
#include <thread>

namespace {

template<typename T, typename F, size_t S>
void test_circular_buffer_read_write() {
    std::array<T, S> src {};

    // Fill source data
    for (size_t i = 0; i < S; ++i) {
        src[i] = static_cast<T>(i + 1);  // NOLINT
    }

    rav::circular_buffer<T, F> buffer(S);

    REQUIRE(buffer.write(src.data(), src.size()));
    REQUIRE_FALSE(buffer.write(&src[0], 1));

    std::array<T, S> dst {};

    REQUIRE(buffer.read(dst.data(), dst.size()));
    REQUIRE(dst == src);
    REQUIRE_FALSE(buffer.read(dst.data(), 1));

    dst = {};

    // No do half write + read to test the wrap around
    static_assert(S % 2 == 0, "Size S must be a multiple of 2.");
    REQUIRE(buffer.write(src.data(), src.size() / 2));
    REQUIRE(buffer.read(dst.data(), src.size() / 2));

    REQUIRE(buffer.write(src.data(), src.size()));
    REQUIRE(buffer.read(dst.data(), dst.size()));

    REQUIRE(dst == src);
}

}  // namespace

TEST_CASE("circular_buffer test basic reading writing") {
    SECTION("Test basic reading and writing") {
        constexpr size_t size = 10;
        test_circular_buffer_read_write<uint8_t, rav::fifo::single, size>();
        test_circular_buffer_read_write<uint16_t, rav::fifo::single, size>();
        test_circular_buffer_read_write<uint32_t, rav::fifo::single, size>();
        test_circular_buffer_read_write<uint64_t, rav::fifo::single, size>();
        test_circular_buffer_read_write<int8_t, rav::fifo::single, size>();
        test_circular_buffer_read_write<int16_t, rav::fifo::single, size>();
        test_circular_buffer_read_write<int32_t, rav::fifo::single, size>();
        test_circular_buffer_read_write<int64_t, rav::fifo::single, size>();

        test_circular_buffer_read_write<uint8_t, rav::fifo::spsc, size>();
        test_circular_buffer_read_write<uint16_t, rav::fifo::spsc, size>();
        test_circular_buffer_read_write<uint32_t, rav::fifo::spsc, size>();
        test_circular_buffer_read_write<uint64_t, rav::fifo::spsc, size>();
        test_circular_buffer_read_write<int8_t, rav::fifo::spsc, size>();
        test_circular_buffer_read_write<int16_t, rav::fifo::spsc, size>();
        test_circular_buffer_read_write<int32_t, rav::fifo::spsc, size>();
        test_circular_buffer_read_write<int64_t, rav::fifo::spsc, size>();

        test_circular_buffer_read_write<uint8_t, rav::fifo::mpsc, size>();
        test_circular_buffer_read_write<uint16_t, rav::fifo::mpsc, size>();
        test_circular_buffer_read_write<uint32_t, rav::fifo::mpsc, size>();
        test_circular_buffer_read_write<uint64_t, rav::fifo::mpsc, size>();
        test_circular_buffer_read_write<int8_t, rav::fifo::mpsc, size>();
        test_circular_buffer_read_write<int16_t, rav::fifo::mpsc, size>();
        test_circular_buffer_read_write<int32_t, rav::fifo::mpsc, size>();
        test_circular_buffer_read_write<int64_t, rav::fifo::mpsc, size>();

        test_circular_buffer_read_write<uint8_t, rav::fifo::spmc, size>();
        test_circular_buffer_read_write<uint16_t, rav::fifo::spmc, size>();
        test_circular_buffer_read_write<uint32_t, rav::fifo::spmc, size>();
        test_circular_buffer_read_write<uint64_t, rav::fifo::spmc, size>();
        test_circular_buffer_read_write<int8_t, rav::fifo::spmc, size>();
        test_circular_buffer_read_write<int16_t, rav::fifo::spmc, size>();
        test_circular_buffer_read_write<int32_t, rav::fifo::spmc, size>();
        test_circular_buffer_read_write<int64_t, rav::fifo::spmc, size>();

        test_circular_buffer_read_write<uint8_t, rav::fifo::mpmc, size>();
        test_circular_buffer_read_write<uint16_t, rav::fifo::mpmc, size>();
        test_circular_buffer_read_write<uint32_t, rav::fifo::mpmc, size>();
        test_circular_buffer_read_write<uint64_t, rav::fifo::mpmc, size>();
        test_circular_buffer_read_write<int8_t, rav::fifo::mpmc, size>();
        test_circular_buffer_read_write<int16_t, rav::fifo::mpmc, size>();
        test_circular_buffer_read_write<int32_t, rav::fifo::mpmc, size>();
        test_circular_buffer_read_write<int64_t, rav::fifo::mpmc, size>();
    }

    SECTION("Test single producer single consumer") {
        constexpr int num_writes_per_thread = 100'000;
        int64_t expected_total = 0;

        rav::circular_buffer<int64_t, rav::fifo::spsc> buffer(10);

        std::thread writer([&] {
            for (int i = 0; i < num_writes_per_thread; i++) {
                const std::array<int64_t, 3> src = {i + 1, i + 2, i + 3};
                while (!buffer.write(src.data(), src.size())) {}

                for (const auto n : src) {
                    expected_total += n;
                }
            }
        });

        int64_t total = 0;

        std::atomic writer_done = false;
        std::thread reader([&] {
            std::array<int64_t, 3> dst = {};

            while (!writer_done) {
                while (buffer.read(dst.data(), dst.size())) {
                    for (const auto n : dst) {
                        total += n;
                    }
                }
            }
        });

        writer.join();

        writer_done = true;

        reader.join();

        REQUIRE(total == expected_total);
    }

    SECTION("Test multi producer single consumer") {
        constexpr int num_writer_threads = 4;
        constexpr int num_writes_per_thread = 100'000;
        std::atomic<int64_t> expected_total = 0;

        rav::circular_buffer<int64_t, rav::fifo::mpsc> buffer(10);

        std::vector<std::thread> writers;
        writers.reserve(num_writer_threads);

        for (auto t = 0; t < num_writer_threads; t++) {
            writers.emplace_back([&] {
                for (int i = 0; i < num_writes_per_thread; i++) {
                    const std::array<int64_t, 3> src = {i + 1, i + 2, i + 3};
                    while (!buffer.write(src.data(), src.size())) {}

                    for (const auto n : src) {
                        expected_total += n;
                    }
                }
            });
        }

        std::atomic<int64_t> total = 0;

        std::atomic writers_done = false;
        std::thread reader([&] {
            std::array<int64_t, 3> dst = {};

            while (!writers_done) {
                while (buffer.read(dst.data(), dst.size())) {
                    for (const auto n : dst) {
                        total.fetch_add(n);
                    }
                }
            }
        });

        for (auto& t : writers) {
            t.join();
        }

        writers_done = true;

        reader.join();

        REQUIRE(total == expected_total);
    }

    SECTION("Test single producer multi consumer") {
        constexpr int num_reader_threads = 4;
        constexpr int num_writes_per_thread = 100'000;
        int64_t expected_total = 0;

        rav::circular_buffer<int64_t, rav::fifo::spmc> buffer(10);

        std::thread writer([&] {
            for (int i = 0; i < num_writes_per_thread; i++) {
                const std::array<int64_t, 3> src = {i + 1, i + 2, i + 3};
                while (!buffer.write(src.data(), src.size())) {}

                for (const auto n : src) {
                    expected_total += n;
                }
            }
        });

        std::atomic writer_done = false;
        std::atomic<int64_t> total = 0;

        std::vector<std::thread> readers;
        readers.reserve(num_reader_threads);

        for (auto t = 0; t < num_reader_threads; t++) {
            readers.emplace_back([&] {
                std::array<int64_t, 3> dst = {};

                while (!writer_done) {
                    while (buffer.read(dst.data(), dst.size())) {
                        for (const auto n : dst) {
                            total.fetch_add(n);
                        }
                    }
                }
            });
        }

        writer.join();

        writer_done = true;

        for (auto& t : readers) {
            t.join();
        }

        REQUIRE(total == expected_total);
    }

    SECTION("Test multi producer multi consumer") {
        constexpr int num_reader_threads = 4;
        constexpr int num_writer_threads = 4;
        constexpr int num_writes_per_thread = 100'000;
        std::atomic<int64_t> expected_total = 0;

        rav::circular_buffer<int64_t, rav::fifo::mpmc> buffer(10);

        std::vector<std::thread> writers;
        writers.reserve(num_writer_threads);

        for (auto t = 0; t < num_writer_threads; t++) {
            writers.emplace_back([&] {
                for (int i = 0; i < num_writes_per_thread; i++) {
                    const std::array<int64_t, 3> src = {i + 1, i + 2, i + 3};
                    while (!buffer.write(src.data(), src.size())) {}

                    for (const auto n : src) {
                        expected_total += n;
                    }
                }
            });
        }

        std::atomic writers_done = false;
        std::atomic<int64_t> total = 0;

        std::vector<std::thread> readers;
        readers.reserve(num_reader_threads);

        for (auto t = 0; t < num_reader_threads; t++) {
            readers.emplace_back([&] {
                std::array<int64_t, 3> dst = {};

                while (!writers_done) {
                    while (buffer.read(dst.data(), dst.size())) {
                        for (const auto n : dst) {
                            total.fetch_add(n);
                        }
                    }
                }
            });
        }

        for (auto& t : writers) {
            t.join();
        }

        writers_done = true;

        for (auto& t : readers) {
            t.join();
        }

        REQUIRE(total == expected_total);
    }
}
