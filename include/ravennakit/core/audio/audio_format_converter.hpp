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
#include "audio_format.hpp"
#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/core/util/todo.hpp"

namespace rav {

class audio_format_converter {
  public:
    audio_format_converter() = default;

    void convert(buffer_view<uint8_t> source, buffer_view<uint8_t> target) {
        TODO("Implement");
    }

    void convert(buffer_view<uint8_t> source, float* const* target_channels, int num_channels, int num_frames) {
        TODO("Implement");
    }

    void convert(const float* const* source_channels, int num_channels, int num_frames, buffer_view<uint8_t> target) {
        TODO("Implement");
    }

    void set_source_format(const audio_format& format) {
        source_format_ = format;
    }

    void set_target_format(const audio_format& format) {
        target_format_ = format;
    }

    [[nodiscard]] const audio_format& get_source_format() const {
        return source_format_;
    }

    [[nodiscard]] const audio_format& get_target_format() const {
        return target_format_;
    }

  private:
    audio_format source_format_;
    audio_format target_format_;
};

}  // namespace rav
