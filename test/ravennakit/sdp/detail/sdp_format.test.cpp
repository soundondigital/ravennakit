/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_format.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::sdp::Format") {
    SECTION("98/L16/48000/2") {
        auto fmt = rav::sdp::parse_format("98 L16/48000/2");
        REQUIRE(fmt);
        REQUIRE(fmt->payload_type == 98);
        REQUIRE(fmt->encoding_name == "L16");
        REQUIRE(fmt->clock_rate == 48000);
        REQUIRE(fmt->num_channels == 2);
        auto audio_format = rav::sdp::make_audio_format(*fmt);
        REQUIRE(audio_format.has_value());
        auto expected_audio_format = rav::AudioFormat {
            rav::AudioFormat::ByteOrder::be, rav::AudioEncoding::pcm_s16,
            rav::AudioFormat::ChannelOrdering::interleaved, 48000, 2
        };
        REQUIRE(*audio_format == expected_audio_format);
    }

    SECTION("98/L16/48000/4") {
        auto fmt = rav::sdp::parse_format("98 L16/48000/4");
        REQUIRE(fmt);
        REQUIRE(fmt->payload_type == 98);
        REQUIRE(fmt->encoding_name == "L16");
        REQUIRE(fmt->clock_rate == 48000);
        REQUIRE(fmt->num_channels == 4);
        auto audio_format = rav::sdp::make_audio_format(*fmt);
        REQUIRE(audio_format.has_value());
        auto expected_audio_format = rav::AudioFormat {
            rav::AudioFormat::ByteOrder::be, rav::AudioEncoding::pcm_s16,
            rav::AudioFormat::ChannelOrdering::interleaved, 48000, 4
        };
        REQUIRE(*audio_format == expected_audio_format);
    }

    SECTION("98/L24/48000/2") {
        auto fmt = rav::sdp::parse_format("98 L24/48000/2");
        REQUIRE(fmt);
        REQUIRE(fmt->payload_type == 98);
        REQUIRE(fmt->encoding_name == "L24");
        REQUIRE(fmt->clock_rate == 48000);
        REQUIRE(fmt->num_channels == 2);
        auto audio_format = rav::sdp::make_audio_format(*fmt);
        REQUIRE(audio_format.has_value());
        auto expected_audio_format = rav::AudioFormat {
            rav::AudioFormat::ByteOrder::be, rav::AudioEncoding::pcm_s24,
            rav::AudioFormat::ChannelOrdering::interleaved, 48000, 2
        };
        REQUIRE(*audio_format == expected_audio_format);
    }

    SECTION("98/L32/48000/2") {
        auto fmt = rav::sdp::parse_format("98 L32/48000/2");
        REQUIRE(fmt);
        REQUIRE(fmt->payload_type == 98);
        REQUIRE(fmt->encoding_name == "L32");
        REQUIRE(fmt->clock_rate == 48000);
        REQUIRE(fmt->num_channels == 2);
        auto audio_format = rav::sdp::make_audio_format(*fmt);
        REQUIRE(audio_format.has_value());
        auto expected_audio_format = rav::AudioFormat {
            rav::AudioFormat::ByteOrder::be, rav::AudioEncoding::pcm_s32,
            rav::AudioFormat::ChannelOrdering::interleaved, 48000, 2
        };
        REQUIRE(*audio_format == expected_audio_format);
    }

    SECTION("98/NA/48000/2") {
        auto fmt = rav::sdp::parse_format("98 NA/48000/2");
        REQUIRE(fmt);
        REQUIRE(fmt->payload_type == 98);
        REQUIRE(fmt->encoding_name == "NA");
        REQUIRE(fmt->clock_rate == 48000);
        REQUIRE(fmt->num_channels == 2);
        auto audio_format = rav::sdp::make_audio_format(*fmt);
        REQUIRE_FALSE(audio_format.has_value());
    }
}
