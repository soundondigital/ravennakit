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
#include "ravennakit/core/todo.hpp"

#include <tuple>

namespace {}

void rav::wav_audio_format::fmt_chunk::read(input_stream& istream, const uint32_t chunk_size) {
    audio_format = istream.read_le<format_code>().value();
    num_channels = istream.read_le<uint16_t>().value();
    sample_rate = istream.read_le<uint32_t>().value();
    avg_bytes_per_sec = istream.read_le<uint32_t>().value();
    block_align = istream.read_le<uint16_t>().value();
    bits_per_sample = istream.read_le<uint16_t>().value();
    if (chunk_size > 16) {
        if (chunk_size < 18) {
            RAV_THROW_EXCEPTION("Invalid fmt chunk size");
        }
        const auto cb_size = istream.read_le<uint16_t>().value();
        if (cb_size > 0) {
            if (chunk_size < 40) {
                RAV_THROW_EXCEPTION("Invalid fmt chunk size");
            }
            extension.emplace();
            extension->cb_size = cb_size;
            extension->valid_bits_per_sample = istream.read_le<uint16_t>().value();
            extension->channel_mask = istream.read_le<uint32_t>().value();
            extension->sub_format = istream.read_le<std::array<uint8_t, 16>>().value();
        }
    }
}

void rav::wav_audio_format::fmt_chunk::write(output_stream& ostream) const {
    ostream.write_string("fmt ");
    if (extension.has_value()) {
        ostream.write_le<uint32_t>(40);
    } else {
        ostream.write_le<uint32_t>(16);
    }
    ostream.write_le(audio_format);
    ostream.write_le(num_channels);
    ostream.write_le(sample_rate);
    ostream.write_le(avg_bytes_per_sec);
    ostream.write_le(block_align);
    ostream.write_le(bits_per_sample);
    if (extension.has_value()) {
        ostream.write_le(extension->cb_size);
        ostream.write_le(extension->valid_bits_per_sample);
        ostream.write_le(extension->channel_mask);
        ostream.write_le(extension->sub_format);
    }
}

void rav::wav_audio_format::data_chunk::read(input_stream& istream, const uint32_t chunk_size) {
    data_begin = istream.get_read_position();
    data_size = chunk_size;
    istream.skip(chunk_size);
}

void rav::wav_audio_format::data_chunk::write(output_stream& ostream) const {
    ostream.write_string("fmt ");
    ostream.write_le<uint32_t>(static_cast<uint32_t>(data_size));
}

rav::wav_audio_format::reader::reader(rav::input_stream& istream) : istream_(istream) {
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
            fmt_chunk_.emplace();
            fmt_chunk_->read(istream, chunk_size.value());
            continue;
        }

        if (chunk_id == "data") {
            data_chunk_.emplace();
            data_chunk_->read(istream, chunk_size.value());
            continue;
        }

        // Skip unknown chunk
        istream.skip(chunk_size.value());
    }
}

size_t rav::wav_audio_format::reader::read_audio_data(uint8_t* buffer, const size_t size) const {
    if (!data_chunk_.has_value()) {
        return 0;
    }

    const auto bytes_to_read = std::min(size, data_chunk_->data_size - data_read_position_);
    if (bytes_to_read == 0) {
        return 0;
    }

    istream_.set_read_position(data_chunk_->data_begin + data_read_position_);
    if (istream_.read(buffer, bytes_to_read) != bytes_to_read) {
        RAV_THROW_EXCEPTION("failed to read audio data");
    }

    return bytes_to_read;
}

double rav::wav_audio_format::reader::sample_rate() const {
    return fmt_chunk_.value().sample_rate;
}

size_t rav::wav_audio_format::reader::num_channels() const {
    return fmt_chunk_.value().num_channels;
}

rav::wav_audio_format::writer::writer(
    output_stream& ostream, const double sample_rate, const size_t num_channels, const size_t bits_per_sample
) :
    ostream_(ostream) {
    fmt_chunk_.sample_rate = static_cast<uint32_t>(sample_rate);
    fmt_chunk_.num_channels = static_cast<uint16_t>(num_channels);
    fmt_chunk_.bits_per_sample = static_cast<uint16_t>(bits_per_sample);
}

size_t rav::wav_audio_format::writer::write_audio_data(const uint8_t* buffer, size_t size) {
    std::ignore = buffer;
    std::ignore = size;
    TODO("Implement");
}

void rav::wav_audio_format::writer::write_header() const {
    ostream_.set_write_position(0);
    ostream_.write_string(" RIFF");
    ostream_.write_le<uint32_t>(0);  // Placeholder for RIFF size
    ostream_.write_string("WAVE");
    fmt_chunk_.write(ostream_);
    data_chunk_.write(ostream_);
}
