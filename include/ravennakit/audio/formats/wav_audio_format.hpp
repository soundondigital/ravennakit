/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/containers/input_stream.hpp"
#include "ravennakit/containers/output_stream.hpp"

#include <ios>
#include <array>

namespace rav::wav_audio_format {

enum class format_code : uint16_t { pcm = 0x1, ieee_float = 0x3, alaw = 0x4, mulaw = 0x7, extensible = 0xfffe };

struct fmt_chunk {
    struct extension {
        uint16_t cb_size {};                      // The size of the extension (in bytes).
        uint16_t valid_bits_per_sample {0xfffe};  // Number of valid bits per sample.
        uint32_t channel_mask {};                 // Speaker position mask.
        std::array<uint8_t, 16> sub_format {};    // GUID, including the data format code.
    };

    format_code audio_format {};         // A number indicating the WAVE format category of the file.
    uint16_t num_channels {};            // The number of channels represented in the waveform data.
    uint32_t sample_rate {};             // The sampling rate (in samples per second).
    uint32_t avg_bytes_per_sec {};       // The average number of bytes per second.
    uint16_t block_align {};             // The block alignment (in bytes) of the waveform data.
    uint16_t bits_per_sample {};         // Bits per sample.
    std::optional<extension> extension;  // Extension.

    void read(input_stream& istream, uint32_t chunk_size);
    void write(output_stream& ostream) const;
};

struct data_chunk {
    size_t data_begin {};
    size_t data_size {};

    void read(input_stream& istream, uint32_t chunk_size);
    void write(output_stream& ostream) const;
};

/**
 * A reader class which reads audio (meta)data from an input stream.
 */
class reader {
  public:
    explicit reader(input_stream& istream);

    size_t read_audio_data(uint8_t* buffer, size_t size) const;

    [[nodiscard]] double sample_rate() const;
    [[nodiscard]] size_t num_channels() const;

  private:
    input_stream& istream_;
    std::optional<fmt_chunk> fmt_chunk_;
    std::optional<data_chunk> data_chunk_;
    size_t data_read_position_ {};
};

/**
 * A writer class which writes audio (meta)data to an output stream.
 */
class writer {
  public:
    explicit writer(output_stream& ostream, double sample_rate, size_t num_channels, size_t bits_per_sample);
    size_t write_audio_data(const uint8_t* buffer, size_t size);

  private:
    output_stream& ostream_;
    fmt_chunk fmt_chunk_;
    data_chunk data_chunk_;

    void write_header() const;
};

}  // namespace rav::wav_audio_format
