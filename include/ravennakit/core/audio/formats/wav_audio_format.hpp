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

#include "ravennakit/core/file.hpp"
#include "ravennakit/core/audio/audio_format.hpp"
#include "ravennakit/core/streams/file_input_stream.hpp"
#include "ravennakit/core/streams/input_stream.hpp"
#include "ravennakit/core/streams/output_stream.hpp"

#include <ios>
#include <array>

namespace rav {

class wav_audio_format {
  public:
    enum class format_code : uint16_t { pcm = 0x1, ieee_float = 0x3, alaw = 0x4, mulaw = 0x7, extensible = 0xfffe };

    /**
     * A struct representing the fmt chunk of a WAVE file.
     */
    struct fmt_chunk {
        struct extension {
            /// The size of the extension (in bytes).
            uint16_t cb_size {};
            /// Number of valid bits per sample.
            uint16_t valid_bits_per_sample {0xfffe};
            /// Speaker position mask.
            uint32_t channel_mask {};
            /// GUID, including the data format code.
            std::array<uint8_t, 16> sub_format {};
        };

        /// A number indicating the WAVE format category of the file.
        format_code format {};
        /// The number of channels represented in the waveform data.
        uint16_t num_channels {};
        /// The sampling rate (in samples per second).
        uint32_t sample_rate {};
        /// The average number of bytes per second.
        uint32_t avg_bytes_per_sec {};
        /// The block alignment (in bytes) of the waveform data.
        uint16_t block_align {};
        /// Bits per sample.
        uint16_t bits_per_sample {};
        /// Extension.
        std::optional<extension> extension;

        /**
         * Reads the fmt chunk from the input stream.
         * @param istream The input stream to read from.
         * @param chunk_size The size of the fmt chunk.
         */
        void read(input_stream& istream, uint32_t chunk_size);

        /**
         * Writes the fmt chunk to the output stream.
         * @param ostream The output stream to write to.
         * @return The number of bytes written.
         */
        [[nodiscard]] tl::expected<size_t, output_stream::error> write(output_stream& ostream) const;

        /**
         * @return The audio format represented by the fmt chunk.
         */
        [[nodiscard]] std::optional<audio_format> to_audio_format() const;
    };

    /**
     * A struct representing the data chunk of a WAVE file.
     */
    struct data_chunk {
        /// The beginning of the audio data in the file, relative to the beginning of the file. Might be 0 if the
        /// position is not yet known. Will be updated when the data is read or written.
        size_t data_begin {};

        /// The size of the audio data in bytes. Will be updated when data is read or written.
        size_t data_size {};

        /**
         * Reads the data chunk from the input stream.
         * @param istream The input stream to read from.
         * @param chunk_size The size of the data chunk.
         */
        [[nodiscard]] bool read(input_stream& istream, uint32_t chunk_size);

        /**
         * Writes the data chunk to the output stream.
         * @param ostream The output stream to write to.
         * @param data_written The number of bytes of audio data written into the stream so far.
         * @return The number of bytes written (excluding the size of the data).
         */
        [[nodiscard]] tl::expected<size_t, output_stream::error> write(output_stream& ostream, size_t data_written);
    };

    /**
     * A reader class which reads audio (meta)data from an input stream.
     */
    class reader {
      public:
        /**
         * Constructs a new reader.
         * @param istream The input stream to read from. Must be valid.
         */
        explicit reader(std::unique_ptr<input_stream> istream);

        reader(reader&&) noexcept = default;
        reader& operator=(reader&&) noexcept = default;

        /**
         * Reads audio data from the input stream.
         * @param buffer The buffer to read data into.
         * @param size The number of bytes to read.
         * @return The number of bytes read.
         */
        [[nodiscard]] tl::expected<size_t, input_stream::error> read_audio_data(uint8_t* buffer, size_t size);

        /**
         * @return The number of bytes of audio data remaining in the stream.
         */
        [[nodiscard]] size_t remaining_audio_data() const;

        /**
         * Sets the read position of the reader. This is the position in the audio data, not the entire file.
         * @param position The position to set the read position to.
         */
        void set_read_position(size_t position);

        /**
         * @return The sample rate of the audio data.
         */
        [[nodiscard]] double sample_rate() const;

        /**
         * @return The number of channels in the audio data.
         */
        [[nodiscard]] size_t num_channels() const;

        /**
         * @return The audio format of the audio data.
         */
        [[nodiscard]] std::optional<audio_format> get_audio_format() const;

      private:
        std::unique_ptr<input_stream> istream_;
        std::optional<fmt_chunk> fmt_chunk_;
        std::optional<data_chunk> data_chunk_;
        size_t data_read_position_ {};
    };

    /**
     * A writer class which writes audio (meta)data to an output stream.
     */
    class writer {
      public:
        explicit writer(
            output_stream& ostream, format_code format, double sample_rate, size_t num_channels, size_t bits_per_sample
        );
        ~writer();

        /**
         * Writes audio data to the output stream.
         * @param buffer The buffer to write data from.
         * @param size The number of bytes to write.
         * @return The number of bytes written.
         */
        [[nodiscard]] tl::expected<void, output_stream::error> write_audio_data(const uint8_t* buffer, size_t size);

        /**
         * Finalizes the WAVE file by writing the header and flushing the output stream.
         * This method can be called multiple times, interleaved with calls to write_audio_data. This can be useful in
         * cases where the computer might lose power, and you don't want to lose the entire file.
         */
        [[nodiscard]] bool finalize();

      private:
        output_stream& ostream_;
        fmt_chunk fmt_chunk_;
        data_chunk data_chunk_;
        size_t audio_data_written_ {};
        size_t chunks_total_size_ {};

        [[nodiscard]] tl::expected<void, rav::output_stream::error> write_header();
    };
};

}  // namespace rav
