/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/audio/formats/wav_audio_format.hpp"

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/exception.hpp"
#include "ravennakit/core/util/todo.hpp"

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

tl::expected<size_t, rav::output_stream::error> rav::wav_audio_format::fmt_chunk::write(output_stream& ostream) const {
    const auto start_pos = ostream.get_write_position();

    auto map_value = [] {
        return 0;
    };

    OK_OR_RETURN(ostream.write("fmt ", 4).map(map_value));  // Note the trailing space
    OK_OR_RETURN(ostream.write_le<uint32_t>(extension.has_value() ? 40 : 16).map(map_value));
    OK_OR_RETURN(ostream.write_le(format).map(map_value));
    OK_OR_RETURN(ostream.write_le(num_channels).map(map_value));
    OK_OR_RETURN(ostream.write_le(sample_rate).map(map_value));
    OK_OR_RETURN(ostream.write_le(avg_bytes_per_sec).map(map_value));
    OK_OR_RETURN(ostream.write_le(block_align).map(map_value));
    OK_OR_RETURN(ostream.write_le(bits_per_sample).map(map_value));

    if (extension.has_value()) {
        OK_OR_RETURN(ostream.write_le(extension->cb_size).map(map_value));
        OK_OR_RETURN(ostream.write_le(extension->valid_bits_per_sample).map(map_value));
        OK_OR_RETURN(ostream.write_le(extension->channel_mask).map(map_value));
        OK_OR_RETURN(ostream.write_le(extension->sub_format).map(map_value));
    }

    return ostream.get_write_position() - start_pos;
}

std::optional<rav::audio_format> rav::wav_audio_format::fmt_chunk::to_audio_format() const {
    switch (format) {
        case format_code::pcm: {
            if (bits_per_sample == 8) {
                return audio_format {
                    audio_format::byte_order::le, audio_encoding::pcm_u8, audio_format::channel_ordering::interleaved,
                    sample_rate, num_channels
                };
            }
            if (bits_per_sample == 16) {
                return audio_format {
                    audio_format::byte_order::le,
                    audio_encoding::pcm_s16,
                    audio_format::channel_ordering::interleaved,
                    sample_rate,
                    num_channels,
                };
            }
            if (bits_per_sample == 24) {
                return audio_format {
                    audio_format::byte_order::le,
                    audio_encoding::pcm_s24,
                    audio_format::channel_ordering::interleaved,
                    sample_rate,
                    num_channels,
                };
            }
            break;
        }
        case format_code::ieee_float: {
            if (bits_per_sample == 32) {
                return audio_format {
                    audio_format::byte_order::le,
                    audio_encoding::pcm_f32,
                    audio_format::channel_ordering::interleaved,
                    sample_rate,
                    num_channels,
                };
            }
            if (bits_per_sample == 64) {
                return audio_format {
                    audio_format::byte_order::le,
                    audio_encoding::pcm_f64,
                    audio_format::channel_ordering::interleaved,
                    sample_rate,
                    num_channels,
                };
            }
            break;
        }
        case format_code::alaw:
        case format_code::mulaw:
        case format_code::extensible:
        default:
            return std::nullopt;
    }
    return std::nullopt;
}

bool rav::wav_audio_format::data_chunk::read(input_stream& istream, const uint32_t chunk_size) {
    data_begin = istream.get_read_position();
    data_size = chunk_size;
    return istream.skip(chunk_size);
}

tl::expected<size_t, rav::output_stream::error>
rav::wav_audio_format::data_chunk::write(output_stream& ostream, const size_t data_written) {
    auto map_value = [] {
        return 0;
    };
    const auto pos = ostream.get_write_position();
    data_size = data_written;
    OK_OR_RETURN(ostream.write("data", 4).map(map_value));
    OK_OR_RETURN(ostream.write_le<uint32_t>(static_cast<uint32_t>(data_size)).map(map_value));
    data_begin = ostream.get_write_position();
    return data_begin - pos;
}

rav::wav_audio_format::reader::reader(std::unique_ptr<input_stream> istream) : istream_(std::move(istream)) {
    RAV_ASSERT(istream_, "Invalid input stream");

    // RIFF header
    const auto riff_header = istream_->read_as_string(4);
    if (riff_header != "RIFF") {
        RAV_THROW_EXCEPTION("expecting RIFF header");
    }

    // RIFF size
    const auto riff_size = istream_->read_le<uint32_t>();
    if (!riff_size.has_value()) {
        RAV_THROW_EXCEPTION("failed to read RIFF size");
    }

    // TODO: Check (validate?) RIFF size

    // WAVE header
    const auto wave_header = istream_->read_as_string(4);
    if (wave_header != "WAVE") {
        RAV_THROW_EXCEPTION("expecting WAVE header");
    }

    // Loop through chunks
    while (!istream_->exhausted()) {
        if (istream_->get_read_position() % 2 == 1) {
            if (!istream_->skip(1)) {
                RAV_THROW_EXCEPTION("failed to skip padding byte");
            }
            if (istream_->exhausted()) {
                break;
            }
        }

        auto chunk_id = istream_->read_as_string(4);
        if (chunk_id.value().size() != 4) {
            RAV_THROW_EXCEPTION("failed to read chunk id");
        }

        const auto chunk_size = istream_->read_le<uint32_t>();
        if (!chunk_size.has_value()) {
            RAV_THROW_EXCEPTION("failed to read chunk size");
        }

        if (chunk_id == "fmt ") {
            fmt_chunk_.emplace();
            fmt_chunk_->read(*istream_, chunk_size.value());
            continue;
        }

        if (chunk_id == "data") {
            data_chunk_.emplace();
            if (!data_chunk_->read(*istream_, chunk_size.value())) {
                RAV_THROW_EXCEPTION("failed to read data chunk");
            }
            continue;
        }

        // Skip unknown chunk
        if (!istream_->skip(chunk_size.value())) {
            RAV_THROW_EXCEPTION("failed to skip chunk");
        }
    }
}

tl::expected<size_t, rav::input_stream::error>
rav::wav_audio_format::reader::read_audio_data(uint8_t* buffer, const size_t size) {
    if (!data_chunk_.has_value()) {
        return 0;
    }

    const auto bytes_to_read = std::min(size, data_chunk_->data_size - data_read_position_);
    if (bytes_to_read == 0) {
        return 0;
    }

    if (!istream_->set_read_position(data_chunk_->data_begin + data_read_position_)) {
        return tl::unexpected(input_stream::error::failed_to_set_read_position);
    }

    auto read = istream_->read(buffer, bytes_to_read);
    if (!read) {
        return read;
    }
    if (read.value() != bytes_to_read) {
        return tl::unexpected(input_stream::error::insufficient_data);
    }

    data_read_position_ += bytes_to_read;

    return bytes_to_read;
}

size_t rav::wav_audio_format::reader::remaining_audio_data() const {
    RAV_ASSERT(data_read_position_ <= data_chunk_->data_size, "Invalid read position");
    return data_chunk_->data_size - data_read_position_;
}

void rav::wav_audio_format::reader::set_read_position(const size_t position) {
    data_read_position_ = position;
}

double rav::wav_audio_format::reader::sample_rate() const {
    return fmt_chunk_.value().sample_rate;
}

size_t rav::wav_audio_format::reader::num_channels() const {
    return fmt_chunk_.value().num_channels;
}

std::optional<rav::audio_format> rav::wav_audio_format::reader::get_audio_format() const {
    if (!fmt_chunk_.has_value()) {
        return std::nullopt;
    }
    return fmt_chunk_->to_audio_format();
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

    if (!write_header()) {
        RAV_THROW_EXCEPTION("failed to write header");
    }
}

rav::wav_audio_format::writer::~writer() {
    if (!finalize()) {
        RAV_ERROR("Failed to finalize wav file");
    }
}

tl::expected<void, rav::output_stream::error>
rav::wav_audio_format::writer::write_audio_data(const uint8_t* buffer, const size_t size) {
    OK_OR_RETURN(ostream_.write(buffer, size));
    audio_data_written_ += size;
    return {};
}

bool rav::wav_audio_format::writer::finalize() {
    if (!write_header()) {
        return false;
    }
    ostream_.flush();
    return true;
}

tl::expected<void, rav::output_stream::error> rav::wav_audio_format::writer::write_header() {
    const auto pos = ostream_.get_write_position();
    OK_OR_RETURN(ostream_.set_write_position(0));
    OK_OR_RETURN(ostream_.write("RIFF", 4));
    // The riff size will only be correct after calling write_header() once before.
    const auto riff_size = chunks_total_size_ + audio_data_written_ + 4;  // +4 for "WAVE"
    RAV_ASSERT(riff_size < std::numeric_limits<uint32_t>::max(), "WAV file too large");
    OK_OR_RETURN(ostream_.write_le<uint32_t>(static_cast<uint32_t>(riff_size)));
    OK_OR_RETURN(ostream_.write("WAVE", 4));
    chunks_total_size_ = fmt_chunk_.write(ostream_).value();                         // TODO: Handle error
    chunks_total_size_ += data_chunk_.write(ostream_, audio_data_written_).value();  // TODO: Handle error

    if (pos > 0) {
        OK_OR_RETURN(ostream_.set_write_position(pos));
    }

    return {};
}
