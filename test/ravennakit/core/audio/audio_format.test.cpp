/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/audio/audio_format.hpp"
#include "audio_format.test.hpp"

#include <nanobench.h>
#include <catch2/catch_all.hpp>

TEST_CASE("rav::AudioFormat") {
    rav::AudioFormat audio_format;
    audio_format.byte_order = rav::AudioFormat::ByteOrder::be;
    audio_format.encoding = rav::AudioEncoding::pcm_s24;
    audio_format.sample_rate = 44100;
    audio_format.num_channels = 2;
    audio_format.ordering = rav::AudioFormat::ChannelOrdering::interleaved;

    test_audio_format_json(audio_format, boost::json::value_from(audio_format));
}

void rav::test_audio_format_json(const AudioFormat& audio_format, const boost::json::value& json) {
    REQUIRE(json.at("byte_order") == AudioFormat::to_string(audio_format.byte_order));
    REQUIRE(json.at("channel_ordering") == AudioFormat::to_string(audio_format.ordering));
    REQUIRE(json.at("encoding") == audio_encoding_to_string(audio_format.encoding));
    REQUIRE(json.at("num_channels") == audio_format.num_channels);
    REQUIRE(json.at("sample_rate") == audio_format.sample_rate);
}
