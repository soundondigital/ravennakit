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

#include <vector>

#include "ravennakit/core/assert.hpp"

namespace rav {

template<class T>
class audio_buffer {
  public:
    audio_buffer() = default;

    /**
     * Constructs an audio buffer with the given number of channels and samples.
     * @param num_channels The number of channels.
     * @param num_samples The number of samples per channel.
     */
    audio_buffer(const size_t num_channels, const size_t num_samples) {
        resize(num_channels, num_samples);
    }

    /**
     * Constructs an audio buffer with the given number of channels and samples and fills it with given value.
     * @param num_channels The number of channels.
     * @param num_samples The number of samples per channel.
     * @param value_to_fill_with The value to fill the buffer with.
     */
    audio_buffer(const size_t num_channels, const size_t num_samples, T value_to_fill_with) {
        resize(num_channels, num_samples);
        std::fill(data_.begin(), data_.end(), value_to_fill_with);
    }

    /**
     * Constructs an audio buffer by copying from another buffer.
     * @param other The other buffer to copy from.
     */
    audio_buffer(const audio_buffer& other) {
        data_ = other.data_;
        channels_.resize(other.channels_.size());
        update_channel_pointers();
    }

    /**
     * Constructs an audio buffer by moving from another buffer.
     * @param other The other buffer to move from.
     */
    audio_buffer(audio_buffer&& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(channels_, other.channels_);
        update_channel_pointers();
    }

    /**
     * Copies the contents from another audio buffer to this buffer.
     * @param other The other buffer to copy from.
     * @return A reference to this buffer.
     */
    audio_buffer& operator=(const audio_buffer& other) {
        data_ = other.data_;
        channels_.resize(other.channels_.size());
        update_channel_pointers();
        return *this;
    }

    audio_buffer& operator=(audio_buffer&& other)  noexcept {
        std::swap(data_, other.data_);
        std::swap(channels_, other.channels_);
        update_channel_pointers();
        other.update_channel_pointers(); // Data is swapped, so we need to update the pointers of the other buffer.
        return *this;
    }

    /**
     * Prepares the audio buffer for the given number of channels and samples. New space will be zero initialized.
     * Existing data will be kept, except if the number of channels or samples is less than the current number of
     * channels or samples.
     * @param num_channels The number of channels.
     * @param num_samples The number of samples per channel.
     */
    void resize(size_t num_channels, const size_t num_samples) {
        if (num_channels == 0 || num_samples == 0) {
            data_.clear();
            channels_.clear();
            return;
        }

        data_.resize(num_channels * num_samples, {});
        channels_.resize(num_channels);

        update_channel_pointers();
    }

    /**
     * @returns An array of pointers to the beginning of each channel which can be written to.
     */
    const T* const* get_array_of_read_pointers() const {
        return channels_.empty() ? nullptr : channels_.data();
    }

    /**
     * @returns An array of pointers to the beginning of each channel.
     */
    T* const* get_array_of_write_pointers() {
        return channels_.empty() ? nullptr : channels_.data();
    }

    /**
     * @returns The number of channels.
     */
    [[nodiscard]] size_t num_channels() const {
        return channels_.size();
    }

    /**
     * @returns The number of samples per channel.
     */
    [[nodiscard]] size_t num_samples() const {
        return channels_.empty() ? 0 : data_.size() / channels_.size();
    }

    /**
     * Sets the value if an individual sample.
     * This method does no bounds checking.
     * @param channel_index The index of the channel.
     * @param sample_index The index of the sample.
     * @param value The value to set.
     */
    void set_sample(size_t channel_index, size_t sample_index, T value) {
        RAV_ASSERT(channel_index < num_channels());
        RAV_ASSERT(sample_index < num_samples());
        channels_[channel_index][sample_index] = value;
    }

    /**
     * Clears the buffer by setting all samples to zero.
     */
    void clear() {
        if constexpr (std::is_unsigned_v<T>) {
            // Use half of the max value of the integral as center value.
            std::fill(data_.begin(), data_.end(), std::numeric_limits<T>::max() / 2 + 1);
        } else {
            std::fill(data_.begin(), data_.end(), T {});
        }
    }

    /**
     * Clears the buffer by setting all samples to the given value.
     * @param value The value to fill the buffer with.
     */
    void clear(T value) {
        std::fill(data_.begin(), data_.end(), value);
    }

    /**
     * Clear a range of samples in a channel.
     * @param channel_index The index of the channel.
     * @param start_sample The index of the first sample to clear.
     * @param num_samples_to_clear The number of samples to clear.
     */
    void clear(size_t channel_index, size_t start_sample, size_t num_samples_to_clear) {
        RAV_ASSERT(channel_index < num_channels());
        RAV_ASSERT(start_sample + num_samples_to_clear <= num_samples());

        if constexpr (std::is_unsigned_v<T>) {
            // Use half of the max value of the integral as center value.
            std::fill(
                channels_[channel_index] + start_sample, channels_[channel_index] + start_sample + num_samples_to_clear,
                std::numeric_limits<T>::max() / 2 + 1
            );
        } else {
            std::fill(
                channels_[channel_index] + start_sample, channels_[channel_index] + start_sample + num_samples_to_clear,
                T {}
            );
        }
    }

    /**
     * Copies data from all channels of src into this all channels of this buffer.
     * @param dst_start_sample The index of the start sample in the destination channel.
     * @param num_samples_to_copy The number of samples to copy.
     * @param src The source data to copy from.
     */
    void copy_from(const size_t dst_start_sample, const size_t num_samples_to_copy, const T* const* src) {
        for (size_t i = 0; i < num_channels(); ++i) {
            copy_from(i, dst_start_sample, num_samples_to_copy, src[i]);
        }
    }

    /**
     * Copies data from src into this buffer.
     * @param dst_channel_index The index of the destination channel.
     * @param dst_start_sample The index of the start sample in the destination channel.
     * @param num_samples_to_copy The number of samples to copy.
     * @param src The source data to copy from.
     */
    void copy_from(
        const size_t dst_channel_index, const size_t dst_start_sample, const size_t num_samples_to_copy, const T* src
    ) {
        RAV_ASSERT(dst_channel_index < num_channels());
        RAV_ASSERT(dst_start_sample + num_samples_to_copy <= num_samples());

        if (num_samples_to_copy == 0) {
            return;
        }

        std::memcpy(channels_[dst_channel_index] + dst_start_sample, src, num_samples_to_copy * sizeof(T));
    }

    /**
     * Copies data from all channels of this buffer into dst.
     * @param dst The destination data to copy to.
     * @param num_channels The number of channels.
     * @param num_samples The number of samples per channel.
     */
    void copy_to(T* const* dst, const size_t num_channels, const size_t num_samples) {
        for (size_t i = 0; i < num_channels; ++i) {
            copy_to(dst[i], i, 0, num_samples);
        }
    }

    /**
     * Copies data from this buffer into dst.
     * @param dst
     * @param src_channel_index
     * @param src_start_sample
     * @param num_samples_to_copy
     */
    void copy_to(T* dst, size_t src_channel_index, size_t src_start_sample, const size_t num_samples_to_copy) {
        RAV_ASSERT(src_channel_index < num_channels());
        RAV_ASSERT(src_start_sample + num_samples_to_copy <= num_samples());

        if (num_samples_to_copy == 0) {
            return;
        }

        std::memcpy(dst, channels_[src_channel_index] + src_start_sample, num_samples_to_copy * sizeof(T));
    }

  private:
    /// Holds the non-interleaved audio data (each channel consecutive).
    std::vector<T> data_;

    /// Holds pointers to the beginning of each channel.
    std::vector<T*> channels_;

    void update_channel_pointers() {
        for (size_t i = 0; i < channels_.size(); ++i) {
            channels_[i] = data_.data() + i * data_.size() / channels_.size();
        }
    }
};

}  // namespace rav
