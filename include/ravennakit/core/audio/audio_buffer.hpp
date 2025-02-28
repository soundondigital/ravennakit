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

#include "audio_buffer_view.hpp"

#include <vector>
#include <cstring>

#include "ravennakit/core/assert.hpp"

namespace rav {

template<class T>
class audio_buffer: public audio_buffer_view<T> {
  public:
    audio_buffer() : audio_buffer_view<T>(nullptr, 0, 0) {}

    /**
     * Constructs an audio buffer with the given number of channels and frames.
     * @param num_channels The number of channels.
     * @param num_frames The number of frames.
     */
    audio_buffer(const size_t num_channels, const size_t num_frames) : audio_buffer_view<T>(nullptr, 0, 0) {
        resize(num_channels, num_frames);
    }

    /**
     * Constructs an audio buffer with the given number of channels and frames and fills it with given value.
     * @param num_channels The number of channels.
     * @param num_frames The number of frames.
     * @param value_to_fill_with The value to fill the buffer with.
     */
    audio_buffer(const size_t num_channels, const size_t num_frames, T value_to_fill_with) :
        audio_buffer_view<T>(nullptr, 0, 0) {
        resize(num_channels, num_frames);
        std::fill(data_.begin(), data_.end(), value_to_fill_with);
    }

    /**
     * Constructs an audio buffer by copying from another buffer.
     * @param other The other buffer to copy from.
     */
    audio_buffer(const audio_buffer& other) : audio_buffer_view<T>(nullptr, 0, 0) {
        data_ = other.data_;
        channels_.resize(other.channels_.size());
        update_channel_pointers();
    }

    /**
     * Constructs an audio buffer by moving from another buffer.
     * @param other The other buffer to move from.
     */
    audio_buffer(audio_buffer&& other) noexcept : audio_buffer_view<T>(nullptr, 0, 0) {
        std::swap(data_, other.data_);
        std::swap(channels_, other.channels_);
        update_channel_pointers();
        other.update_channel_pointers();
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

  private:
    /// Holds the non-interleaved audio data (each channel consecutive).
    std::vector<T> data_;

    /// Holds pointers to the beginning of each channel.
    std::vector<T*> channels_;

    void update_channel_pointers() {
        for (size_t i = 0; i < channels_.size(); ++i) {
            channels_[i] = data_.data() + i * data_.size() / channels_.size();
        }
        audio_buffer_view<T>::update(channels_.data(), channels_.size(), data_.size() / channels_.size());
    }


};

}  // namespace rav
