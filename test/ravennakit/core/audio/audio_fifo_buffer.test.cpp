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
#include <ravennakit/core/audio/audio_fifo_buffer.hpp>
#include <thread>

#include "ravennakit/core/containers/vector_buffer.hpp"

namespace {

constexpr size_t k_num_channels = 2;
constexpr int k_num_reader_threads = 4;
constexpr int k_num_writer_threads = 4;
constexpr int k_num_writes_per_thread = 10'000;

template<class T, class F>
void instantiate_buffer() {
    rav::AudioFifoBuffer<T, F> buffer;
}

template<class T>
rav::AudioBuffer<T> get_filled_audio_buffer(const size_t num_channels, const size_t num_frames) {
    rav::AudioBuffer<T> buffer {num_channels, num_frames};
    T counter = 1;
    for (size_t ch = 0; ch < num_channels; ++ch) {
        for (size_t fr = 0; fr < num_frames; ++fr) {
            buffer[ch][fr] = counter++;
        }
    }
    return buffer;
}

template<typename T>
int64_t get_sum_of_all_samples(const rav::AudioBuffer<T>& buffer) {
    int64_t total = 0;
    for (size_t ch = 0; ch < buffer.num_channels(); ++ch) {
        for (size_t fr = 0; fr < buffer.num_frames(); ++fr) {
            total += buffer[ch][fr];
        }
    }
    return total;
}

template<typename T, typename F>
void test_circular_buffer_read_write() {
    rav::AudioFifoBuffer<T, F> buffer {2, 10};

    const auto src = get_filled_audio_buffer<T>(k_num_channels, 3);
    REQUIRE(buffer.write(src));
    REQUIRE(buffer.write(src));
    REQUIRE(buffer.write(src));
    REQUIRE_FALSE(buffer.write(src));

    rav::AudioBuffer<T> dst {k_num_channels, 9};
    REQUIRE(buffer.read(dst));
    REQUIRE_FALSE(buffer.read(dst));

    REQUIRE(dst[0][0] == 1);
    REQUIRE(dst[0][1] == 2);
    REQUIRE(dst[0][2] == 3);
    REQUIRE(dst[1][0] == 4);
    REQUIRE(dst[1][1] == 5);
    REQUIRE(dst[1][2] == 6);

    REQUIRE(dst[0][3] == 1);
    REQUIRE(dst[0][4] == 2);
    REQUIRE(dst[0][5] == 3);
    REQUIRE(dst[1][3] == 4);
    REQUIRE(dst[1][4] == 5);
    REQUIRE(dst[1][5] == 6);

    REQUIRE(dst[0][6] == 1);
    REQUIRE(dst[0][7] == 2);
    REQUIRE(dst[0][8] == 3);
    REQUIRE(dst[1][6] == 4);
    REQUIRE(dst[1][7] == 5);
    REQUIRE(dst[1][8] == 6);

    REQUIRE(buffer.write(src));
    REQUIRE(buffer.write(src));
    REQUIRE(buffer.write(src));
    REQUIRE_FALSE(buffer.write(src));

    REQUIRE(buffer.read(dst));
    REQUIRE_FALSE(buffer.read(dst));

    REQUIRE(dst[0][0] == 1);
    REQUIRE(dst[0][1] == 2);
    REQUIRE(dst[0][2] == 3);
    REQUIRE(dst[1][0] == 4);
    REQUIRE(dst[1][1] == 5);
    REQUIRE(dst[1][2] == 6);

    REQUIRE(dst[0][3] == 1);
    REQUIRE(dst[0][4] == 2);
    REQUIRE(dst[0][5] == 3);
    REQUIRE(dst[1][3] == 4);
    REQUIRE(dst[1][4] == 5);
    REQUIRE(dst[1][5] == 6);

    REQUIRE(dst[0][6] == 1);
    REQUIRE(dst[0][7] == 2);
    REQUIRE(dst[0][8] == 3);
    REQUIRE(dst[1][6] == 4);
    REQUIRE(dst[1][7] == 5);
    REQUIRE(dst[1][8] == 6);
}

}  // namespace

TEST_CASE("rav::CircularAudioBuffer") {
    SECTION("Buffers holding different types should be able to be created") {
        instantiate_buffer<int, rav::Fifo::Single>();
        instantiate_buffer<int, rav::Fifo::Spsc>();
        instantiate_buffer<int, rav::Fifo::Mpsc>();
        instantiate_buffer<int, rav::Fifo::Spmc>();
        instantiate_buffer<int, rav::Fifo::Mpmc>();

        instantiate_buffer<float, rav::Fifo::Single>();
        instantiate_buffer<float, rav::Fifo::Spsc>();
        instantiate_buffer<float, rav::Fifo::Mpsc>();
        instantiate_buffer<float, rav::Fifo::Spmc>();
        instantiate_buffer<float, rav::Fifo::Mpmc>();

        instantiate_buffer<double, rav::Fifo::Single>();
        instantiate_buffer<double, rav::Fifo::Spsc>();
        instantiate_buffer<double, rav::Fifo::Mpsc>();
        instantiate_buffer<double, rav::Fifo::Spmc>();
        instantiate_buffer<double, rav::Fifo::Mpmc>();
    }

    SECTION("Basic read write tests") {
        test_circular_buffer_read_write<uint8_t, rav::Fifo::Single>();
        test_circular_buffer_read_write<uint16_t, rav::Fifo::Single>();
        test_circular_buffer_read_write<uint32_t, rav::Fifo::Single>();
        test_circular_buffer_read_write<uint64_t, rav::Fifo::Single>();
        test_circular_buffer_read_write<int8_t, rav::Fifo::Single>();
        test_circular_buffer_read_write<int16_t, rav::Fifo::Single>();
        test_circular_buffer_read_write<int32_t, rav::Fifo::Single>();
        test_circular_buffer_read_write<int64_t, rav::Fifo::Single>();

        test_circular_buffer_read_write<uint8_t, rav::Fifo::Spsc>();
        test_circular_buffer_read_write<uint16_t, rav::Fifo::Spsc>();
        test_circular_buffer_read_write<uint32_t, rav::Fifo::Spsc>();
        test_circular_buffer_read_write<uint64_t, rav::Fifo::Spsc>();
        test_circular_buffer_read_write<int8_t, rav::Fifo::Spsc>();
        test_circular_buffer_read_write<int16_t, rav::Fifo::Spsc>();
        test_circular_buffer_read_write<int32_t, rav::Fifo::Spsc>();
        test_circular_buffer_read_write<int64_t, rav::Fifo::Spsc>();

        test_circular_buffer_read_write<uint8_t, rav::Fifo::Mpsc>();
        test_circular_buffer_read_write<uint16_t, rav::Fifo::Mpsc>();
        test_circular_buffer_read_write<uint32_t, rav::Fifo::Mpsc>();
        test_circular_buffer_read_write<uint64_t, rav::Fifo::Mpsc>();
        test_circular_buffer_read_write<int8_t, rav::Fifo::Mpsc>();
        test_circular_buffer_read_write<int16_t, rav::Fifo::Mpsc>();
        test_circular_buffer_read_write<int32_t, rav::Fifo::Mpsc>();
        test_circular_buffer_read_write<int64_t, rav::Fifo::Mpsc>();

        test_circular_buffer_read_write<uint8_t, rav::Fifo::Spmc>();
        test_circular_buffer_read_write<uint16_t, rav::Fifo::Spmc>();
        test_circular_buffer_read_write<uint32_t, rav::Fifo::Spmc>();
        test_circular_buffer_read_write<uint64_t, rav::Fifo::Spmc>();
        test_circular_buffer_read_write<int8_t, rav::Fifo::Spmc>();
        test_circular_buffer_read_write<int16_t, rav::Fifo::Spmc>();
        test_circular_buffer_read_write<int32_t, rav::Fifo::Spmc>();
        test_circular_buffer_read_write<int64_t, rav::Fifo::Spmc>();

        test_circular_buffer_read_write<uint8_t, rav::Fifo::Mpmc>();
        test_circular_buffer_read_write<uint16_t, rav::Fifo::Mpmc>();
        test_circular_buffer_read_write<uint32_t, rav::Fifo::Mpmc>();
        test_circular_buffer_read_write<uint64_t, rav::Fifo::Mpmc>();
        test_circular_buffer_read_write<int8_t, rav::Fifo::Mpmc>();
        test_circular_buffer_read_write<int16_t, rav::Fifo::Mpmc>();
        test_circular_buffer_read_write<int32_t, rav::Fifo::Mpmc>();
        test_circular_buffer_read_write<int64_t, rav::Fifo::Mpmc>();
    }

    SECTION("Test single producer single consumer") {
        int64_t expected_total = 0;

        rav::AudioFifoBuffer<int, rav::Fifo::Spsc> buffer(2, 10);

        std::thread writer([&] {
            const auto src = get_filled_audio_buffer<int>(2, 3);
            for (int i = 0; i < k_num_writes_per_thread; i++) {
                while (!buffer.write(src)) {}
                expected_total += get_sum_of_all_samples(src);
            }
        });

        int64_t total = 0;

        std::atomic writer_done = false;
        std::thread reader([&] {
            rav::AudioBuffer<int> dst(2, 3);
            while (!writer_done) {
                while (buffer.read(dst)) {
                    total += get_sum_of_all_samples(dst);
                }
            }
        });

        writer.join();

        writer_done = true;

        reader.join();

        REQUIRE(total == expected_total);
    }

    SECTION("Test multi producer single consumer") {
        std::atomic<int64_t> expected_total = 0;

        rav::AudioFifoBuffer<int, rav::Fifo::Mpsc> buffer(2, 10);

        std::vector<std::thread> writers;
        writers.reserve(k_num_writer_threads);

        for (auto t = 0; t < k_num_writer_threads; t++) {
            writers.emplace_back([&] {
                const auto src = get_filled_audio_buffer<int>(2, 3);
                for (int i = 0; i < k_num_writes_per_thread; i++) {
                    while (!buffer.write(src)) {}
                    expected_total += get_sum_of_all_samples(src);
                }
            });
        }

        std::atomic<int64_t> total = 0;

        std::atomic writers_done = false;
        std::thread reader([&] {
            rav::AudioBuffer<int> dst(2, 3);
            while (!writers_done) {
                while (buffer.read(dst)) {
                    total += get_sum_of_all_samples(dst);
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
        int64_t expected_total = 0;

        rav::AudioFifoBuffer<int, rav::Fifo::Spmc> buffer(2, 10);

        std::thread writer([&] {
            const auto src = get_filled_audio_buffer<int>(2, 3);
            for (int i = 0; i < k_num_writes_per_thread; i++) {
                while (!buffer.write(src)) {}
                expected_total += get_sum_of_all_samples(src);
            }
        });

        std::atomic writer_done = false;
        std::atomic<int64_t> total = 0;

        std::vector<std::thread> readers;
        readers.reserve(k_num_reader_threads);

        for (auto t = 0; t < k_num_reader_threads; t++) {
            readers.emplace_back([&] {
                rav::AudioBuffer<int> dst(2, 3);
                while (!writer_done) {
                    while (buffer.read(dst)) {
                        total += get_sum_of_all_samples(dst);
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
        std::atomic<int64_t> expected_total = 0;

        rav::AudioFifoBuffer<int, rav::Fifo::Mpmc> buffer(2, 10);

        std::vector<std::thread> writers;
        writers.reserve(k_num_writer_threads);

        for (auto t = 0; t < k_num_writer_threads; t++) {
            writers.emplace_back([&] {
                const auto src = get_filled_audio_buffer<int>(2, 3);
                for (int i = 0; i < k_num_writes_per_thread; i++) {
                    while (!buffer.write(src)) {}
                    expected_total += get_sum_of_all_samples(src);
                }
            });
        }

        std::atomic writers_done = false;
        std::atomic<int64_t> total = 0;

        std::vector<std::thread> readers;
        readers.reserve(k_num_reader_threads);

        for (auto t = 0; t < k_num_reader_threads; t++) {
            readers.emplace_back([&] {
                rav::AudioBuffer<int> dst(2, 3);
                while (!writers_done) {
                    while (buffer.read(dst)) {
                        total += get_sum_of_all_samples(dst);
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

    SECTION("Read from data") {
        rav::VectorBuffer<int16_t> src({1, 2, 3, 4, 5, 6});
        rav::AudioFifoBuffer<int16_t> ring(2, 5);

        auto result =
            ring.write_from_data<int16_t, rav::AudioData::ByteOrder::Ne, rav::AudioData::Interleaving::Interleaved>(
                src.data(), 3
            );

        REQUIRE(result);

        rav::AudioBuffer<int16_t> dst(2, 3);
        auto read_result = ring.read(dst);
        REQUIRE(read_result);
        REQUIRE(dst[0][0] == 1);
        REQUIRE(dst[0][1] == 3);
        REQUIRE(dst[0][2] == 5);
        REQUIRE(dst[1][0] == 2);
        REQUIRE(dst[1][1] == 4);
        REQUIRE(dst[1][2] == 6);

        result =
            ring.write_from_data<int16_t, rav::AudioData::ByteOrder::Ne, rav::AudioData::Interleaving::Interleaved>(
                src.data(), 3
            );

        REQUIRE(result);

        dst.clear();
        read_result = ring.read(dst);
        REQUIRE(read_result);
        REQUIRE(dst[0][0] == 1);
        REQUIRE(dst[0][1] == 3);
        REQUIRE(dst[0][2] == 5);
        REQUIRE(dst[1][0] == 2);
        REQUIRE(dst[1][1] == 4);
        REQUIRE(dst[1][2] == 6);
    }

    SECTION("Write to data") {
        rav::VectorBuffer<int16_t> src({1, 2, 3, 4, 5, 6});
        rav::AudioFifoBuffer<int16_t> ring(2, 5);

        auto result =
            ring.write_from_data<int16_t, rav::AudioData::ByteOrder::Ne, rav::AudioData::Interleaving::Interleaved>(
                src.data(), 3
            );

        REQUIRE(result);

        std::vector<int16_t> dst(6);
        result = ring.read_to_data<int16_t, rav::AudioData::ByteOrder::Ne, rav::AudioData::Interleaving::Interleaved>(
            dst.data(), 3
        );

        REQUIRE(result);

        REQUIRE(dst[0] == 1);
        REQUIRE(dst[1] == 2);
        REQUIRE(dst[2] == 3);
        REQUIRE(dst[3] == 4);
        REQUIRE(dst[4] == 5);
        REQUIRE(dst[5] == 6);
    }
}
