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
    constexpr auto sin_1ms_wav_header_size = 44;
    const auto sin_1ms_wav_audio_data_size = sin_1ms_wav.size() - sin_1ms_wav_header_size;
    rav::byte_stream bytes;
    {
        rav::wav_audio_format::writer writer(bytes, rav::wav_audio_format::format_code::pcm, 44100, 2, 16);
        writer.write_audio_data(sin_1ms_wav.data() + sin_1ms_wav_header_size, sin_1ms_wav_audio_data_size);
        // Let writer go out of scope to let it finalize the file (in the destructor).
    }

    // 44 is the size of the header of the current implementation.
    constexpr auto header_size = 44;
    REQUIRE(bytes.size() == sin_1ms_wav_audio_data_size + header_size);

    REQUIRE(bytes.read_as_string(4) == "RIFF");
    REQUIRE(bytes.read_le<uint32_t>().value() == 1800);
    REQUIRE(bytes.read_as_string(4) == "WAVE");
    REQUIRE(bytes.read_as_string(4) == "fmt ");
    REQUIRE(bytes.read_le<uint32_t>().value() == 16);      // fmt chunk size
    REQUIRE(bytes.read_le<uint16_t>().value() == 0x1);     // Format code
    REQUIRE(bytes.read_le<uint16_t>().value() == 2);       // Num channels
    REQUIRE(bytes.read_le<uint32_t>().value() == 44100);   // Sample rate
    REQUIRE(bytes.read_le<uint32_t>().value() == 176400);  // Avg bytes per sec
    REQUIRE(bytes.read_le<uint16_t>().value() == 4);       // Block align
    REQUIRE(bytes.read_le<uint16_t>().value() == 16);      // Bits per sample
    REQUIRE(bytes.read_as_string(4) == "data");
    REQUIRE(bytes.read_le<uint32_t>().value() == 1764);  // Data size

    std::vector<uint8_t> read_audio_data(1764, 0);
    REQUIRE(bytes.read(read_audio_data.data(), read_audio_data.size()) == 1764);
    REQUIRE(std::equal(
        sin_1ms_wav.begin() + sin_1ms_wav_header_size, sin_1ms_wav.end(), read_audio_data.begin(), read_audio_data.end()
    ));
}
