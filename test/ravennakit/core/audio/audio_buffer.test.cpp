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
#include <ravennakit/core/audio/audio_buffer.hpp>
#include <ravennakit/core/util.hpp>

namespace {
struct custom_sample_type {
    size_t channel_index;
    size_t sample_index;
};

template<class T>
void check_sample_values(const rav::audio_buffer<T>& buffer, const T& fill_value) {
    auto* const* audio_data = buffer.data();

    if (buffer.num_channels() == 0 || buffer.num_frames() == 0) {
        FAIL("Buffer has no data");
    }

    for (size_t ch = 0; ch < buffer.num_channels(); ch++) {
        for (size_t sample = 0; sample < buffer.num_frames(); sample++) {
            if constexpr (std::is_floating_point_v<T>) {
                if (!rav::util::is_within(audio_data[ch][sample], fill_value, T {})) {
                    FAIL("Sample value is not within tolerance.");
                }
            } else {
                if (audio_data[ch][sample] != fill_value) {
                    FAIL("Sample value is not equal to the fill value.");
                }
            }
        }
    }
}

// Helper function to create a test buffer with a given number of channels and frames. The values of the samples will
// be an increasing sequence starting from 1.
template<class T>
rav::audio_buffer<T> get_test_buffer(const size_t num_channels, const size_t num_frames) {  // NOLINT
    rav::audio_buffer<T> buffer(num_channels, num_frames);

    int value = 1;
    for (size_t ch = 0; ch < num_channels; ch++) {
        for (size_t sample = 0; sample < num_frames; sample++) {
            buffer.set_sample(ch, sample, value++);
        }
    }

    return buffer;
}

template<class T>
void test_audio_buffer_clear_for_type(T expected_cleared_value) {
    size_t num_channels = 3;
    size_t num_frames = 4;

    {
        rav::audio_buffer<T> buffer(num_channels, num_frames, 1);
        check_sample_values<T>(buffer, 1);
        buffer.clear();
        check_sample_values(buffer, expected_cleared_value);
    }

    {
        rav::audio_buffer<T> buffer(num_channels, num_frames, 1);
        check_sample_values<T>(buffer, 1);
        buffer.clear(expected_cleared_value);
        check_sample_values(buffer, expected_cleared_value);
    }

    {
        rav::audio_buffer<T> buffer(num_channels, num_frames, 1);
        check_sample_values<T>(buffer, 1);
        for (size_t ch = 0; ch < num_channels; ch++) {
            buffer.clear(ch, 0, num_frames);
        }
        check_sample_values(buffer, expected_cleared_value);
    }
}

}  // namespace

TEST_CASE("audio_buffer") {
    SECTION("audio_buffer::audio_buffer()") {
        SECTION(
            "Instantiate different buffer types to get an error when the buffer cannot be used for a particular type."
        ) {
            { rav::audio_buffer<float> buffer; }
            { rav::audio_buffer<double> buffer; }

            { rav::audio_buffer<int8_t> buffer; }
            { rav::audio_buffer<int16_t> buffer; }
            { rav::audio_buffer<int32_t> buffer; }
            { rav::audio_buffer<int64_t> buffer; }

            { rav::audio_buffer<uint8_t> buffer; }
            { rav::audio_buffer<uint16_t> buffer; }
            { rav::audio_buffer<uint32_t> buffer; }
            { rav::audio_buffer<uint64_t> buffer; }
        }

        SECTION("Empty buffer state") {
            rav::audio_buffer<float> buffer {0, 0};

            // When the buffer holds no data, we expect nullptrs.
            REQUIRE(buffer.data() == nullptr);
            REQUIRE(buffer.num_channels() == 0);
            REQUIRE(buffer.num_frames() == 0);
        }

        SECTION("Initial state with some buffers") {
            rav::audio_buffer<int> buffer(2, 5);
            REQUIRE(buffer.num_channels() == 2);
            REQUIRE(buffer.num_frames() == 5);
            check_sample_values(buffer, 0);
        }

        SECTION("Prepare buffer") {
            rav::audio_buffer<int> buffer;
            buffer.resize(2, 3);
            REQUIRE(buffer.num_channels() == 2);
            REQUIRE(buffer.num_frames() == 3);
            check_sample_values(buffer, 0);
        }

        SECTION("Construct and fill with value") {
            static constexpr size_t channel_sizes[] = {1, 2, 3, 512};
            static constexpr size_t sample_sizes[] = {1, 2, 128, 256};
            static constexpr int fill_value = 42;

            for (auto channel_size : channel_sizes) {
                for (auto sample_size : sample_sizes) {
                    rav::audio_buffer buffer(channel_size, sample_size, fill_value);
                    REQUIRE(buffer.num_channels() == channel_size);
                    REQUIRE(buffer.num_frames() == sample_size);

                    check_sample_values(buffer, fill_value);
                }
            }
        }
    }

    SECTION("audio_buffer::set_sample()") {
        constexpr size_t num_channels = 3;
        constexpr size_t num_frames = 4;

        rav::audio_buffer<custom_sample_type> buffer(num_channels, num_frames);

        for (size_t ch = 0; ch < num_channels; ch++) {
            for (size_t sample = 0; sample < num_frames; sample++) {
                buffer.set_sample(ch, sample, custom_sample_type {ch, sample});
            }
        }

        auto* const* audio_data = buffer.data();

        for (size_t ch = 0; ch < num_channels; ch++) {
            for (size_t sample = 0; sample < num_frames; sample++) {
                REQUIRE(audio_data[ch][sample].channel_index == ch);
                REQUIRE(audio_data[ch][sample].sample_index == sample);
            }
        }
    }

    SECTION("audio_buffer::clear()") {
        test_audio_buffer_clear_for_type<float>(0.0f);
        test_audio_buffer_clear_for_type<double>(0.0);
        test_audio_buffer_clear_for_type<int8_t>(0);
        test_audio_buffer_clear_for_type<int16_t>(0);
        test_audio_buffer_clear_for_type<int32_t>(0);
        test_audio_buffer_clear_for_type<int64_t>(0);
        test_audio_buffer_clear_for_type<uint8_t>(128u);
        test_audio_buffer_clear_for_type<uint16_t>(32768u);
        test_audio_buffer_clear_for_type<uint32_t>(2147483648u);
        test_audio_buffer_clear_for_type<uint64_t>(9223372036854775808u);
    }

    SECTION("audio_buffer::copy_from()") {
        size_t num_channels = 2;
        size_t num_frames = 3;

        SECTION("Single channel") {
            rav::audio_buffer<int> buffer(num_channels, num_frames);

            int channel0[3] = {1, 2, 3};
            int channel1[3] = {4, 5, 6};

            buffer.copy_from(0, 0, num_frames, channel0);
            buffer.copy_from(1, 0, num_frames, channel1);

            auto* const* audio_data = buffer.data();

            REQUIRE(audio_data[0][0] == 1);
            REQUIRE(audio_data[0][1] == 2);
            REQUIRE(audio_data[0][2] == 3);
            REQUIRE(audio_data[1][0] == 4);
            REQUIRE(audio_data[1][1] == 5);
            REQUIRE(audio_data[1][2] == 6);
        }

        SECTION("Multiple channels") {
            rav::audio_buffer<int> src(num_channels, num_frames);
            src[0][0] = 1;
            src[0][1] = 2;
            src[0][2] = 3;
            src[1][0] = 4;
            src[1][1] = 5;
            src[1][2] = 6;

            rav::audio_buffer<int> dst(num_channels, num_frames);

            dst.copy_from(0, num_frames, src.data(), src.num_channels());

            REQUIRE(dst[0][0] == 1);
            REQUIRE(dst[0][1] == 2);
            REQUIRE(dst[0][2] == 3);
            REQUIRE(dst[1][0] == 4);
            REQUIRE(dst[1][1] == 5);
            REQUIRE(dst[1][2] == 6);

            dst.copy_from(1, num_frames - 1, src.data(), src.num_channels());

            REQUIRE(dst[0][0] == 1);
            REQUIRE(dst[0][1] == 1);
            REQUIRE(dst[0][2] == 2);
            REQUIRE(dst[1][0] == 4);
            REQUIRE(dst[1][1] == 4);
            REQUIRE(dst[1][2] == 5);

            dst.clear();
            dst.copy_from(0, num_frames - 1, src.data(), src.num_channels(), 1);

            REQUIRE(dst[0][0] == 2);
            REQUIRE(dst[0][1] == 3);
            REQUIRE(dst[0][2] == 0);
            REQUIRE(dst[1][0] == 5);
            REQUIRE(dst[1][1] == 6);
            REQUIRE(dst[1][2] == 0);
        }
    }

    SECTION("audio_buffer::copy_to()") {
        constexpr size_t num_channels = 2;
        constexpr size_t num_frames = 3;

        SECTION("Single channel") {
            auto buffer = get_test_buffer<int>(num_channels, num_frames);

            int channel0[num_frames] = {};
            int channel1[num_frames] = {};

            buffer.copy_to(0, 0, num_frames, channel0);
            buffer.copy_to(1, 0, num_frames, channel1);

            REQUIRE(channel0[0] == 1);
            REQUIRE(channel0[1] == 2);
            REQUIRE(channel0[2] == 3);
            REQUIRE(channel1[0] == 4);
            REQUIRE(channel1[1] == 5);
            REQUIRE(channel1[2] == 6);
        }

        SECTION("Multiple channels") {
            auto buffer = get_test_buffer<int>(num_channels, num_frames);

            rav::audio_buffer<int> dst(num_channels, num_frames);

            buffer.copy_to(0, num_frames, dst.data(), num_channels);

            REQUIRE(dst[0][0] == 1);
            REQUIRE(dst[0][1] == 2);
            REQUIRE(dst[0][2] == 3);
            REQUIRE(dst[1][0] == 4);
            REQUIRE(dst[1][1] == 5);
            REQUIRE(dst[1][2] == 6);

            dst.clear();
            buffer.copy_to(1, num_frames - 1, dst.data(), num_channels);

            REQUIRE(dst[0][0] == 2);
            REQUIRE(dst[0][1] == 3);
            REQUIRE(dst[0][2] == 0);
            REQUIRE(dst[1][0] == 5);
            REQUIRE(dst[1][1] == 6);
            REQUIRE(dst[1][2] == 0);

            dst.clear();
            buffer.copy_to(0, num_frames - 1, dst.data(), num_channels, 1);

            REQUIRE(dst[0][0] == 0);
            REQUIRE(dst[0][1] == 1);
            REQUIRE(dst[0][2] == 2);
            REQUIRE(dst[1][0] == 0);
            REQUIRE(dst[1][1] == 4);
            REQUIRE(dst[1][2] == 5);
        }

        SECTION("Single channel not all samples") {
            auto buffer = get_test_buffer<int>(num_channels, num_frames);

            int channel0[num_frames] = {};
            int channel1[num_frames] = {};

            buffer.copy_to(0, 1, num_frames - 1, channel0 + 1);
            buffer.copy_to(1, 1, num_frames - 1, channel1 + 1);

            REQUIRE(channel0[0] == 0);
            REQUIRE(channel0[1] == 2);
            REQUIRE(channel0[2] == 3);
            REQUIRE(channel1[0] == 0);
            REQUIRE(channel1[1] == 5);
            REQUIRE(channel1[2] == 6);
        }
    }

    SECTION("audio_buffer::audio_buffer(copy)") {
        constexpr size_t num_channels = 2;
        constexpr size_t num_frames = 3;

        const auto buffer = get_test_buffer<int>(num_channels, num_frames);

        const rav::audio_buffer<int> copy(buffer);  // NOLINT

        auto buffer_data = buffer.data();
        auto copy_data = copy.data();

        // Pointers should be different
        REQUIRE(buffer_data != copy_data);
        REQUIRE(buffer_data[0] != copy_data[0]);
        REQUIRE(buffer_data[1] != copy_data[1]);

        // But the data should be the same
        REQUIRE(buffer == copy);
    }

    SECTION("audio_buffer::operator=(copy)") {
        constexpr size_t num_channels = 2;
        constexpr size_t num_frames = 3;

        const auto buffer = get_test_buffer<int>(num_channels, num_frames);

        rav::audio_buffer<int> copy = buffer;  // NOLINT

        auto buffer_data = buffer.data();
        auto copy_data = copy.data();

        // Pointers should be different
        REQUIRE(buffer_data != copy_data);
        REQUIRE(buffer_data[0] != copy_data[0]);
        REQUIRE(buffer_data[1] != copy_data[1]);

        // But the data should be the same
        REQUIRE(buffer == copy);
    }

    SECTION("audio_buffer::audio_buffer(move)") {
        constexpr size_t num_channels = 2;
        constexpr size_t num_frames = 3;

        auto buffer = get_test_buffer<int>(num_channels, num_frames);

        auto copy = rav::audio_buffer<int>(std::move(buffer));  // NOLINT

        REQUIRE(buffer.num_channels() == 0);
        REQUIRE(buffer.num_frames() == 0);

        REQUIRE(buffer.data() == nullptr);
        auto copy_data = copy.data();

        REQUIRE(copy_data[0][0] == 1);
        REQUIRE(copy_data[0][1] == 2);
        REQUIRE(copy_data[0][2] == 3);
        REQUIRE(copy_data[1][0] == 4);
        REQUIRE(copy_data[1][1] == 5);
        REQUIRE(copy_data[1][2] == 6);
    }

    SECTION("audio_buffer::operator=(move)") {
        SECTION("Basic test") {
            constexpr size_t num_channels = 2;
            constexpr size_t num_frames = 3;

            auto buffer = get_test_buffer<int>(num_channels, num_frames);
            rav::audio_buffer<int> copy;
            copy = std::move(buffer);

            REQUIRE(buffer.num_channels() == 0);
            REQUIRE(buffer.num_frames() == 0);

            REQUIRE(buffer.data() == nullptr);
            auto copy_data = copy.data();

            REQUIRE(copy_data[0][0] == 1);
            REQUIRE(copy_data[0][1] == 2);
            REQUIRE(copy_data[0][2] == 3);
            REQUIRE(copy_data[1][0] == 4);
            REQUIRE(copy_data[1][1] == 5);
            REQUIRE(copy_data[1][2] == 6);
        }

        SECTION("Test move-swapping") {
            constexpr size_t num_channels = 2;
            constexpr size_t num_frames = 3;

            auto buffer = get_test_buffer<int>(num_channels, num_frames);
            rav::audio_buffer<int> copy(num_channels, num_frames, 5);
            copy = std::move(buffer);

            REQUIRE(buffer.num_channels() == num_channels);
            REQUIRE(buffer.num_frames() == num_frames);

            auto buffer_data = buffer.data();

            REQUIRE(buffer_data[0][0] == 5);
            REQUIRE(buffer_data[0][1] == 5);
            REQUIRE(buffer_data[0][2] == 5);
            REQUIRE(buffer_data[1][0] == 5);
            REQUIRE(buffer_data[1][1] == 5);
            REQUIRE(buffer_data[1][2] == 5);

            REQUIRE(buffer.data() != copy.data());
            auto copy_data = copy.data();

            REQUIRE(copy_data[0][0] == 1);
            REQUIRE(copy_data[0][1] == 2);
            REQUIRE(copy_data[0][2] == 3);
            REQUIRE(copy_data[1][0] == 4);
            REQUIRE(copy_data[1][1] == 5);
            REQUIRE(copy_data[1][2] == 6);
        }
    }

    SECTION("audio_buffer::operator==()") {
        constexpr size_t num_frames = 3;
        constexpr size_t num_channels = 2;

        SECTION("Change a sample value") {
            auto lhs = get_test_buffer<int>(num_channels, num_frames);
            auto rhs = get_test_buffer<int>(num_channels, num_frames);

            REQUIRE(lhs == rhs);
            REQUIRE_FALSE(lhs != rhs);

            lhs.set_sample(0, 0, 42);

            REQUIRE_FALSE(lhs == rhs);
            REQUIRE(lhs != rhs);
        }

        SECTION("Different number of channels") {
            auto lhs = get_test_buffer<int>(num_channels, num_frames);
            auto rhs = get_test_buffer<int>(num_channels + 1, num_frames);

            REQUIRE_FALSE(lhs == rhs);
            REQUIRE(lhs != rhs);
        }
    }

    SECTION("audio_buffer::add") {
        constexpr float eps = 0.000001f;

        SECTION("Basic operation") {
            auto buffer1 = get_test_buffer<float>(2, 5);
            auto buffer2 = get_test_buffer<float>(2, 5);

            REQUIRE(buffer1.add(buffer2));

            REQUIRE_THAT(buffer1[0][0], Catch::Matchers::WithinRel(2.f, eps));
            REQUIRE_THAT(buffer1[0][1], Catch::Matchers::WithinRel(4.f, eps));
            REQUIRE_THAT(buffer1[0][2], Catch::Matchers::WithinRel(6.f, eps));
            REQUIRE_THAT(buffer1[0][3], Catch::Matchers::WithinRel(8.f, eps));
            REQUIRE_THAT(buffer1[0][4], Catch::Matchers::WithinRel(10.f, eps));
            REQUIRE_THAT(buffer1[1][0], Catch::Matchers::WithinRel(12.f, eps));
            REQUIRE_THAT(buffer1[1][1], Catch::Matchers::WithinRel(14.f, eps));
            REQUIRE_THAT(buffer1[1][2], Catch::Matchers::WithinRel(16.f, eps));
            REQUIRE_THAT(buffer1[1][3], Catch::Matchers::WithinRel(18.f, eps));
            REQUIRE_THAT(buffer1[1][4], Catch::Matchers::WithinRel(20.f, eps));
        }

        SECTION("Channels frames mismatch 1") {
            auto buffer1 = get_test_buffer<float>(1, 5);
            auto buffer2 = get_test_buffer<float>(2, 5);
            REQUIRE_FALSE(buffer1.add(buffer2));
        }

        SECTION("Channels frames mismatch 2") {
            auto buffer1 = get_test_buffer<float>(3, 5);
            auto buffer2 = get_test_buffer<float>(2, 5);
            REQUIRE_FALSE(buffer1.add(buffer2));
        }

        SECTION("Channels frames mismatch 3") {
            auto buffer1 = get_test_buffer<float>(2, 6);
            auto buffer2 = get_test_buffer<float>(2, 5);
            REQUIRE_FALSE(buffer1.add(buffer2));
        }

        SECTION("Channels frames mismatch 4") {
            auto buffer1 = get_test_buffer<float>(2, 4);
            auto buffer2 = get_test_buffer<float>(2, 5);
            REQUIRE_FALSE(buffer1.add(buffer2));
        }
    }
}
