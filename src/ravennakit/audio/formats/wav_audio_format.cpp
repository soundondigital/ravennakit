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

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/exception.hpp"
#include "ravennakit/core/todo.hpp"

#include <tuple>

namespace {}

void rav::wav_audio_format::fmt_chunk::read(input_stream& istream, const uint32_t chunk_size) {
    format = istream.read_le<format_code>().value();
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

size_t rav::wav_audio_format::fmt_chunk::write(output_stream& ostream) const {
    const auto start_pos = ostream.get_write_position();
    ostream.write_cstring("fmt ", 4);
    if (extension.has_value()) {
        ostream.write_le<uint32_t>(40);
    } else {
        ostream.write_le<uint32_t>(16);
    }
    ostream.write_le(format);
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
    const auto written = ostream.get_write_position() - start_pos;
    return written;
}

void rav::wav_audio_format::data_chunk::read(input_stream& istream, const uint32_t chunk_size) {
    data_begin = istream.get_read_position();
    data_size = chunk_size;
    istream.skip(chunk_size);
}

size_t rav::wav_audio_format::data_chunk::write(output_stream& ostream, const size_t data_written) {
    const auto start_pos = ostream.get_write_position();
    data_size = data_written;
    ostream.write_cstring("data", 4);
    ostream.write_le<uint32_t>(static_cast<uint32_t>(data_size));
    data_begin = ostream.get_write_position();
    const auto s = data_begin - start_pos;
    return s;
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
    output_stream& ostream, const format_code format, const double sample_rate, const size_t num_channels,
    const size_t bits_per_sample
) :
    ostream_(ostream) {
    fmt_chunk_.format = format;
    fmt_chunk_.sample_rate = static_cast<uint32_t>(sample_rate);
    fmt_chunk_.num_channels = static_cast<uint16_t>(num_channels);
    fmt_chunk_.avg_bytes_per_sec =
        static_cast<uint32_t>(fmt_chunk_.sample_rate * fmt_chunk_.num_channels * bits_per_sample / 8);
    fmt_chunk_.block_align = static_cast<uint16_t>(fmt_chunk_.num_channels * bits_per_sample / 8);
    fmt_chunk_.bits_per_sample = static_cast<uint16_t>(bits_per_sample);

    write_header();
}

rav::wav_audio_format::writer::~writer() {
    finalize();
}

size_t rav::wav_audio_format::writer::write_audio_data(const uint8_t* buffer, const size_t size) {
    const auto written = ostream_.write(buffer, size);
    audio_data_written_ += written;
    return written;
}

void rav::wav_audio_format::writer::finalize() {
    write_header();
    ostream_.flush();
}

void rav::wav_audio_format::writer::write_header() {
    const auto pos = ostream_.get_write_position();
    ostream_.set_write_position(0);

    ostream_.write_cstring("RIFF", 4);
    // The riff size will only be correct after calling write_header() once before.
    const auto riff_size = fmt_chunk_size_ + data_chunk_size_ + audio_data_written_ + 4;  // +4 for "WAVE"
    RAV_ASSERT(riff_size < std::numeric_limits<uint32_t>::max(), "WAV file too large");
    ostream_.write_le<uint32_t>(static_cast<uint32_t>(riff_size));
    ostream_.write_cstring("WAVE", 4);
    fmt_chunk_size_ = fmt_chunk_.write(ostream_);
    data_chunk_size_ = data_chunk_.write(ostream_, audio_data_written_);

    if (pos > 0) {
        ostream_.set_write_position(pos);
    }
}
