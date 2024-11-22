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

#include <ravennakit/core/containers/detail/fifo.hpp>

#include "audio_buffer.hpp"
#include "audio_data.hpp"

#include <cstring>

namespace rav {

template<class T, class F = fifo::single>
class circular_audio_buffer {
  public:
    circular_audio_buffer() = default;

    /**
     * Constructs a queue with a given number of elements.
     * @param num_channels The number of channels in the buffer.
     * @param num_frames The number of frames in the buffer.
     */
    explicit circular_audio_buffer(const size_t num_channels, const size_t num_frames) {
        resize(num_channels, num_frames);
    }

    /**
     * Writes audio data to the buffer.
     * @param src Source data.
     * @return True if writing was successful, false if there was not enough space to write all data.
     */
    bool write(const audio_buffer<T>& src) {
        return write(src.data(), src.num_channels(), src.num_frames());
    }

    /**
     * Writes audio data to the buffer.
     * @param src Source data.
     * @param num_channels Number of channels in the source data.
     * @param number_of_frames Number of frames in the source data.
     * @return True if writing was successful, false if there was not enough space to write all data.
     */
    bool write(const T* const* src, const size_t num_channels, const size_t number_of_frames) {
        if (auto lock = fifo_.prepare_for_write(number_of_frames)) {
            buffer_.copy_from(lock.position.index1, lock.position.size1, src, num_channels);

            if (lock.position.size2 > 0) {
                buffer_.copy_from(0, lock.position.size2, src, num_channels, lock.position.size1);
            }

            lock.commit();

            return true;
        }

        return false;
    }

    /**
     * Reads audio data from the buffer.
     * @param dst Destination buffer.
     * @return True if reading was successful, false if there was not enough data to read.
     */
    bool read(audio_buffer<T>& dst) {
        return read(dst.data(), dst.num_channels(), dst.num_frames());
    }

    /**
     * Reads audio data from the buffer.
     * @param dst Destination buffers.
     * @param num_channels Number of channels in the destination buffers.
     * @param number_of_frames Number of frames to read.
     * @return True if reading was successful, false if there was not enough data to read.
     */
    bool read(T* const* dst, const size_t num_channels, const size_t number_of_frames) {
        if (auto lock = fifo_.prepare_for_read(number_of_frames)) {
            buffer_.copy_to(lock.position.index1, lock.position.size1, dst, num_channels);

            if (lock.position.size2 > 0) {
                buffer_.copy_to(0, lock.position.size2, dst, num_channels, lock.position.size1);
            }

            lock.commit();

            return true;
        }

        return false;
    }

    /**
     * Writes audio data into the buffer, converting the source data to the buffer's format.
     * @tparam SrcType The type of the source data.
     * @tparam SrcByteOrder The byte order of the source data.
     * @tparam SrcInterleaving The interleaving of the source data.
     * @param data The destination buffer.
     * @param num_frames The number of frames to read and write.
     * @return True if reading was successful, false if there was not enough data to read.
     */
    template<class SrcType, class SrcByteOrder, class SrcInterleaving>
    bool write_from_data(const SrcType* data, const size_t num_frames) {
        RAV_ASSERT(buffer_.num_channels() > 0, "Buffer must have channels");
        RAV_ASSERT(data != nullptr, "Data must not be null");
        auto num_channels = buffer_.num_channels();

        if (auto write = fifo_.prepare_for_write(num_frames)) {
            audio_data::convert<SrcType, SrcByteOrder, SrcInterleaving, T, audio_data::byte_order::ne>(
                data, write.position.size1, num_channels, buffer_.data(), 0, write.position.index1
            );

            if (write.position.size2 > 0) {
                audio_data::convert<SrcType, SrcByteOrder, SrcInterleaving, T, audio_data::byte_order::ne>(
                    data, write.position.size2, num_channels, buffer_.data(), write.position.size1, 0
                );
            }

            write.commit();

            return true;
        }

        return false;
    }

    /**
     * Reads audio data from the buffer, converting the data to the destination format.
     * @tparam DstType The type of the destination data.
     * @tparam DstByteOrder The byte order of the destination data.
     * @tparam DstInterleaving The interleaving of the destination data.
     * @param data The destination buffer.
     * @param num_frames The number of frames to read and write.
     * @return True if reading was successful, false if there was not enough data to read.
     */
    template<class DstType, class DstByteOrder, class DstInterleaving>
    bool read_to_data(DstType* data, const size_t num_frames) {
        RAV_ASSERT(buffer_.num_channels() > 0, "Buffer must have channels");
        RAV_ASSERT(data != nullptr, "Data must not be null");
        auto num_channels = buffer_.num_channels();

        if (auto read = fifo_.prepare_for_read(num_frames)) {
            audio_data::convert<T, audio_data::byte_order::ne, DstType, DstByteOrder, DstInterleaving>(
                buffer_.data(), read.position.size1, num_channels, data, read.position.index1, 0
            );

            if (read.position.size2 > 0) {
                audio_data::convert<T, audio_data::byte_order::ne, DstType, DstByteOrder, DstInterleaving>(
                    buffer_.data(), read.position.size2, num_channels, data, 0, read.position.size1
                );
            }

            read.commit();

            return true;
        }

        return false;
    }

    /**
     * Resizes this buffer, clearing existing data.
     * @param num_channels The number of channels in the buffer.
     * @param num_frames The number of frames in the buffer.
     */
    void resize(const size_t num_channels, const size_t num_frames) {
        buffer_.resize(num_channels, num_frames);
        fifo_.resize(num_frames);
    }

    /**
     * @returns The number of channels.
     */
    [[nodiscard]] size_t num_channels() const {
        return buffer_.num_channels();
    }

    /**
     * @returns The number of frames (samples per channel).
     */
    [[nodiscard]] size_t num_frames() const {
        return buffer_.num_frames();
    }

    /**
     * Clears the buffer.
     */
    void reset() {
        buffer_.clear();
        fifo_.reset();
    }

    /**
     * @returns The number of elements in the buffer.
     */
    size_t size() {
        return fifo_.size();
    }

  private:
    audio_buffer<T> buffer_;
    F fifo_;
};

}  // namespace rav
