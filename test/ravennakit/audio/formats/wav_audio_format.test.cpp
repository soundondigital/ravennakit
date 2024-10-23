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

#include "wav_audio_format.data.cpp"
#include "ravennakit/audio/formats/wav_audio_format.hpp"
#include "ravennakit/containers/byte_stream.hpp"
#include "ravennakit/core/util.hpp"

TEST_CASE("wav_audio_format | Read wav file", "[wav_audio_format]") {
    REQUIRE(sin_1ms_wav.size() == 1808);

    rav::byte_stream bytes(sin_1ms_wav);
    REQUIRE(bytes.size().value() == 1808);

    rav::wav_audio_format::reader reader(bytes);
    REQUIRE(reader.num_channels() == 2);
    REQUIRE(rav::util::is_within(reader.sample_rate(), 44100.0, 0.00001));

    std::vector<uint8_t> read_audio_data(1764, 0);
    const auto read = reader.read_audio_data(read_audio_data.data(), read_audio_data.size());
    REQUIRE(read == 1764);

    // Compare whether the audio data is the same
    REQUIRE(std::equal(sin_1ms_wav.begin() + 44, sin_1ms_wav.end(), read_audio_data.begin(), read_audio_data.end()));
}

TEST_CASE("wav_audio_format | Write wav file", "[wav_audio_format]") {
    rav::byte_stream bytes;
    rav::wav_audio_format::writer writer(bytes, 44100, 2, 16);

}
