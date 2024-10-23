/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/audio/formats/wav_audio_format.hpp"
#include "ravennakit/core/exception.hpp"

#include <utility>

namespace {}

void rav::wav_audio_format::fmt_chunk::parse(input_stream& istream, const uint32_t chunk_size) {
    audio_format = istream.read_le<format_code>().value();
    num_channels = istream.read_le<uint16_t>().value();
    sample_rate = istream.read_le<uint32_t>().value();
    avg_bytes_per_sec = istream.read_le<uint32_t>().value();
    block_align = istream.read_le<uint16_t>().value();
    bits_per_sample = istream.read_le<uint16_t>().value();
    if (chunk_size > 16) {
        cb_size = istream.read_le<uint16_t>().value();
    }
    if (cb_size > 0) {
        valid_bits_per_sample = istream.read_le<uint16_t>().value();
        channel_mask = istream.read_le<uint32_t>().value();
        sub_format = istream.read_le<std::array<uint8_t, 16>>().value();
    }
}

rav::wav_audio_format::reader::reader(input_stream& istream) {
    // RIFF header
    const auto riff_header = istream.read_as_string(4);
    if (riff_header != "RIFF") {
        RAV_THROW_EXCEPTION("expecting RIFF header");
    }

    // RIFF size
    const auto riff_size = istream.read_le<uint32_t>();
    if (!riff_size.has_value()) {
        RAV_THROW_EXCEPTION("failed to read RIFF size");
    }

    // TODO: Check (validate?) RIFF size

    // WAVE header
    const auto wave_header = istream.read_as_string(4);
    if (wave_header != "WAVE") {
        RAV_THROW_EXCEPTION("expecting WAVE header");
    }

    // Loop through chunks
    while (!istream.exhausted()) {
        auto chunk_id = istream.read_as_string(4);
        if (chunk_id.size() != 4) {
            RAV_THROW_EXCEPTION("failed to read chunk id");
        }

        const auto chunk_size = istream.read_le<uint32_t>();
        if (!chunk_size.has_value()) {
            RAV_THROW_EXCEPTION("failed to read chunk size");
        }

        if (chunk_id == "fmt ") {
            fmt_chunk_.parse(istream, chunk_size.value());
            continue;
        }

        if (chunk_id == "data") {
            data_begin_ = istream.get_read_position();
            data_size_ = chunk_size.value();
            istream.skip(data_size_);
            continue;
        }

        // Skip unknown chunk
        istream.skip(chunk_size.value());
    }
}

rav::wav_audio_format::format_code rav::wav_audio_format::reader::format() const {
    return fmt_chunk_.audio_format;
}

uint16_t rav::wav_audio_format::reader::num_channels() const {
    return fmt_chunk_.num_channels;
}

uint32_t rav::wav_audio_format::reader::sample_rate() const {
    return fmt_chunk_.sample_rate;
}

rav::wav_audio_format::writer::writer(std::ostream& istream) {
    std::ignore = istream;
}
