/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/streams/input_stream_view.hpp"

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/util.hpp"

rav::input_stream_view::input_stream_view(const uint8_t* data, const size_t size) : data_(data), size_(size) {
    RAV_ASSERT(data != nullptr, "Data must not be nullptr");
    RAV_ASSERT(size > 0, "Size must be greater than 0");
    if (data_ == nullptr) {
        size_ = 0;
    }
}

void rav::input_stream_view::reset() {
    read_position_ = 0;
}

tl::expected<size_t, rav::input_stream::error> rav::input_stream_view::read(uint8_t* buffer, const size_t size) {
    if (size_ - read_position_ < size) {
        return 0;
    }
    std::memcpy(buffer, data_ + read_position_, size);
    read_position_ += size;
    return size;
}

bool rav::input_stream_view::set_read_position(const size_t position) {
    if (position > size_) {
        return false;
    }
    read_position_ = position;
    return true;
}

size_t rav::input_stream_view::get_read_position() {
    return read_position_;
}

std::optional<size_t> rav::input_stream_view::size() const {
    return size_;
}

bool rav::input_stream_view::exhausted() {
    return read_position_ >= size_;
}
