/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once
#include "ravennakit/core/assert.hpp"

namespace rav {

/**
 * A non-owning view of a non-interleaved audio buffer.
 * @tparam T
 */
template<class T>
class audio_buffer_view {
  public:
    /**
     * Constructs an audio buffer view with the given channels, number of channels, and number of frames. The view does
     * not take ownership or take copies of the data.
     * @param channels The channels.
     * @param num_channels The number of channels.
     * @param num_frames The number of frames.
     */
    audio_buffer_view(T* const* channels, const size_t num_channels, const size_t num_frames) :
        channels_(channels), num_channels_(num_channels), num_frames_(num_frames) {}

    /**
     * Accesses the channel at the given index.
     * @param channel_index The index of the channel to get.
     * @return A pointer to the beginning of the channel.
     */
    const T* operator[](size_t channel_index) const {
        RAV_ASSERT(channel_index < audio_buffer_view<T>::num_channels(), "Channel index out of bounds");
        return channels_[channel_index];
    }

    /**
     * Compares two audio buffers for equality. It does this by comparing the actual contents. If both audio buffers are
     * empty, they are considered equal.
     * @param lhs Left hand side audio buffer.
     * @param rhs Right hand side audio buffer.
     * @return True if the audio buffers are equal, false otherwise.
     */
    friend bool operator==(const audio_buffer_view& lhs, const audio_buffer_view& rhs) {
        if (lhs.num_channels_ != rhs.num_channels_ || lhs.num_frames_ != rhs.num_frames_) {
            return false;
        }
        for (size_t ch = 0; ch < lhs.num_channels_; ++ch) {
            if (!std::equal(lhs.channels_[ch], lhs.channels_[ch] + lhs.num_frames_, rhs.channels_[ch])) {
                return false;
            }
        }
        return true;
    }

    /**
     * Compares two audio buffers for inequality. It does this by comparing the actual contents. If both audio buffers
     * are empty, they are considered equal.
     * @param lhs Left hand side audio buffer.
     * @param rhs Right hand side audio buffer.
     * @return True if the audio buffers are not equal, false otherwise.
     */
    friend bool operator!=(const audio_buffer_view& lhs, const audio_buffer_view& rhs) {
        return !(lhs == rhs);
    }

    /**
     * @returns The number of channels.
     */
    [[nodiscard]] size_t num_channels() const {
        return num_channels_;
    }

    /**
     * @returns The number of frames (samples per channel).
     */
    [[nodiscard]] size_t num_frames() const {
        return num_frames_;
    }

    /**
     * Accesses the channel at the given index.
     * Does not perform bounds checking.
     * @param channel_index The index of the channel to get.
     * @return A pointer to the beginning of the channel.
     */
    T* operator[](size_t channel_index) {
        RAV_ASSERT(channel_index < num_channels_, "Channel index out of bounds");
        return channels_[channel_index];
    }

    /**
     * @returns An array of pointers to the beginning of each channel. Might be nullptr if the view is empty.
     */
    const T* const* data() const {
        return channels_;
    }

    /**
     * @returns An array of pointers to the beginning of each channel which can be written to. Might be nullptr if the
     * view is empty.
     */
    T* const* data() {
        return channels_;
    }

    /**
     * Sets the value of an individual sample.
     * This method does no bounds checking.
     * @param channel_index The index of the channel.
     * @param frame_index The index of the sample.
     * @param value The value to set.
     */
    void set_sample(size_t channel_index, size_t frame_index, T value) {
        RAV_ASSERT(channel_index < num_channels_, "Channel index out of bounds");
        RAV_ASSERT(frame_index < num_frames_, "Frame index out of bounds");
        channels_[channel_index][frame_index] = value;
    }

    /**
     * Clears the buffer by setting all samples to zero.
     */
    void clear() {
        for (size_t i = 0; i < num_channels_; ++i) {
            clear_audio_data(channels_[i], channels_[i] + num_frames_);
        }
    }

    /**
     * Clears the buffer by setting all samples to the given value.
     * @param value The value to fill the buffer with.
     */
    void clear(T value) {
        if (channels_ == nullptr) {
            return;
        }
        for (size_t i = 0; i < num_channels_; ++i) {
            std::fill(channels_[i], channels_[i] + num_frames_, value);
        }
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

  protected:
    /**
     * Updates the channel pointers, number of channels and number of frames.
     * @param channels The channels.
     * @param num_channels The number of channels.
     * @param num_frames The number of frames.
     */
    void update(T* const* channels, const size_t num_channels, const size_t num_frames) {
        channels_ = channels;
        num_channels_ = num_channels;
        num_frames_ = num_frames;
    }

  private:
    T* const* channels_ {};
    size_t num_channels_ {};
    size_t num_frames_ {};

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
