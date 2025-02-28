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
#include <cstring>

#include "ravennakit/core/assert.hpp"

namespace rav {

template<class T>
class audio_buffer {
  public:
    audio_buffer() = default;

    /**
     * Constructs an audio buffer with the given number of channels and frames.
     * @param num_channels The number of channels.
     * @param num_frames The number of frames.
     */
    audio_buffer(const size_t num_channels, const size_t num_frames) {
        resize(num_channels, num_frames);
    }

    /**
     * Constructs an audio buffer with the given number of channels and frames and fills it with given value.
     * @param num_channels The number of channels.
     * @param num_frames The number of frames.
     * @param value_to_fill_with The value to fill the buffer with.
     */
    audio_buffer(const size_t num_channels, const size_t num_frames, T value_to_fill_with) {
        resize(num_channels, num_frames);
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

    /**
     * Moves the contents from another audio buffer to this buffer. It does this by swapping, so other will have the
     * contents of this buffer.
     * @param other The other buffer to move from.
     * @return A reference to this buffer.
     */
    audio_buffer& operator=(audio_buffer&& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(channels_, other.channels_);
        update_channel_pointers();
        other.update_channel_pointers();  // Data is swapped, so we need to update the pointers of the other buffer.
        return *this;
    }

    /**
     * Compares two audio buffers for equality. It does this by comparing the data and the number of channels.
     * @param lhs Left hand side audio buffer.
     * @param rhs Right hand side audio buffer.
     * @return True if the audio buffers are equal, false otherwise.
     */
    friend bool operator==(const audio_buffer& lhs, const audio_buffer& rhs) {
        if (lhs.channels_.size() != rhs.channels_.size()) {
            return false;
        }
        return lhs.data_ == rhs.data_;
    }

    /**
     * Compares two audio buffers for inequality. It does this by comparing the data and the number of channels.
     * @param lhs Left hand side audio buffer.
     * @param rhs Right hand side audio buffer.
     * @return True if the audio buffers are not equal, false otherwise.
     */
    friend bool operator!=(const audio_buffer& lhs, const audio_buffer& rhs) {
        return !(lhs == rhs);
    }

    /**
     * Accesses the channel at the given index.
     * @param channel_index The index of the channel to get.
     * @return A pointer to the beginning of the channel.
     */
    T* operator[](size_t channel_index) {
        RAV_ASSERT(channel_index < num_channels(), "Channel index out of bounds");
        return channels_[channel_index];
    }

    /**
     * Accesses the channel at the given index.
     * @param channel_index The index of the channel to get.
     * @return A pointer to the beginning of the channel.
     */
    const T* operator[](size_t channel_index) const {
        RAV_ASSERT(channel_index < num_channels(), "Channel index out of bounds");
        return channels_[channel_index];
    }

    /**
     * Prepares the audio buffer for the given number of channels and frames. New space will be zero initialized.
     * Existing data will be kept, except if the number of channels or frames is less than the current number of
     * channels or frames.
     * @param num_channels The number of channels.
     * @param num_frames The number of frames.
     */
    void resize(size_t num_channels, const size_t num_frames) {
        if (num_channels == 0 || num_frames == 0) {
            data_.clear();
            channels_.clear();
            return;
        }

        data_.resize(num_channels * num_frames, {});
        channels_.resize(num_channels);

        update_channel_pointers();
    }

    /**
     * @returns An array of pointers to the beginning of each channel which can be written to.
     */
    const T* const* data() const {
        return channels_.empty() ? nullptr : channels_.data();
    }

    /**
     * @returns An array of pointers to the beginning of each channel.
     */
    T* const* data() {
        return channels_.empty() ? nullptr : channels_.data();
    }

    /**
     * @returns The number of channels.
     */
    [[nodiscard]] size_t num_channels() const {
        return channels_.size();
    }

    /**
     * @returns The number of frames (samples per channel).
     */
    [[nodiscard]] size_t num_frames() const {
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
        RAV_ASSERT(channel_index < num_channels(), "Channel index out of bounds");
        RAV_ASSERT(sample_index < num_frames(), "Sample index out of bounds");
        channels_[channel_index][sample_index] = value;
    }

    /**
     * Clears the buffer by setting all samples to zero.
     */
    void clear() {
        clear_audio_data(data_.begin(), data_.end());
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
     * @param start_sample The index of the first frame to clear.
     * @param num_samples_to_clear The number of samples to clear.
     */
    void clear(const size_t channel_index, const size_t start_sample, const size_t num_samples_to_clear) {
        RAV_ASSERT(channel_index < num_channels(), "Channel index out of bounds");
        RAV_ASSERT(start_sample + num_samples_to_clear <= num_frames(), "Sample index out of bounds");
        clear_audio_data(
            channels_[channel_index] + start_sample, channels_[channel_index] + start_sample + num_samples_to_clear
        );
    }

    /**
     * Copies data from all channels of src into all channels of this buffer.
     * @param dst_start_frame The index of the start frame.
     * @param num_frames_to_copy The number of frames to copy.
     * @param src The source data to copy from.
     * @param src_num_channels The number of channels in the source data.
     * @param src_start_frame The index of the start frame in the source data.
     */
    void copy_from(
        const size_t dst_start_frame, const size_t num_frames_to_copy, const T* const* src,
        const size_t src_num_channels, const size_t src_start_frame = 0
    ) {
        RAV_ASSERT(src_num_channels == num_channels(), "Number of channels mismatch");
        for (size_t i = 0; i < std::min(src_num_channels, num_channels()); ++i) {
            copy_from(i, dst_start_frame, num_frames_to_copy, src[i] + src_start_frame);
        }
    }

    /**
     * Copies data from src into this buffer.
     * @param dst_channel_index The index of the destination channel.
     * @param dst_start_sample The index of the start frame in the destination channel.
     * @param num_samples_to_copy The number of samples to copy.
     * @param src The source data to copy from.
     */
    void copy_from(
        const size_t dst_channel_index, const size_t dst_start_sample, const size_t num_samples_to_copy, const T* src
    ) {
        RAV_ASSERT(dst_channel_index < num_channels(), "Channel index out of bounds");
        RAV_ASSERT(dst_start_sample + num_samples_to_copy <= num_frames(), "Sample index out of bounds");

        if (num_samples_to_copy == 0) {
            return;
        }

        std::memcpy(channels_[dst_channel_index] + dst_start_sample, src, num_samples_to_copy * sizeof(T));
    }

    /**
     * Copies data from all channels of this buffer into dst.
     * @param num_frames The number of frames.
     * @param src_start_frame The index of the start frame in the source data.
     * @param dst The destination data to copy to.
     * @param dst_num_channels The number of channels.
     * @param dst_start_frame The index of the start frame in the destination data.
     */
    void copy_to(
        const size_t src_start_frame, const size_t num_frames, T* const* dst, const size_t dst_num_channels,
        const size_t dst_start_frame = 0
    ) {
        RAV_ASSERT(dst_num_channels == num_channels(), "Number of channels mismatch");
        for (size_t i = 0; i < std::min(num_channels(), dst_num_channels); ++i) {
            copy_to(i, src_start_frame, num_frames, dst[i] + dst_start_frame);
        }
    }

    /**
     * Copies data from this buffer into dst.
     * @param src_channel_index The index of the source channel.
     * @param src_start_sample The index of the start frame in the source channel.
     * @param num_samples_to_copy The number of samples to copy.
     * @param dst The destination data to copy to.
     */
    void
    copy_to(const size_t src_channel_index, const size_t src_start_sample, const size_t num_samples_to_copy, T* dst) {
        RAV_ASSERT(src_channel_index < num_channels(), "Channel index out of bounds");
        RAV_ASSERT(src_start_sample + num_samples_to_copy <= num_frames(), "Sample index out of bounds");

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

    template<class First, class Last>
    void clear_audio_data(First first, Last last) {
        if constexpr (std::is_unsigned_v<T>) {
            std::fill(first, last, std::numeric_limits<T>::max() / 2 + 1);
        } else {
            std::fill(first, last, T {});
        }
    }
};

}  // namespace rav
