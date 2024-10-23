/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/containers/byte_stream.hpp"

rav::byte_stream::byte_stream(std::vector<uint8_t> data) : data_(std::move(data)), write_position_(data_.size()) {}

size_t rav::byte_stream::read(uint8_t* buffer, const size_t size) {
    if (exhausted()) {
        return 0;
    }

    const size_t bytes_to_read = std::min(size, data_.size() - read_position_);
    std::memcpy(buffer, data_.data() + read_position_, bytes_to_read);
    read_position_ += bytes_to_read;
    return bytes_to_read;
}

bool rav::byte_stream::set_read_position(const size_t position) {
    if (position > data_.size()) {
        return false;
    }
    read_position_ = position;
    return true;
}

size_t rav::byte_stream::get_read_position() {
    return read_position_;
}

std::optional<size_t> rav::byte_stream::size() const {
    return data_.size();
}

bool rav::byte_stream::exhausted() const {
    return read_position_ >= data_.size();
}

size_t rav::byte_stream::write(const uint8_t* buffer, const size_t size) {
    if (write_position_ + size > data_.size()) {
        data_.resize(write_position_ + size, 0);
    }

    std::memcpy(data_.data() + write_position_, buffer, size);
    write_position_ += size;
    return size;
}

bool rav::byte_stream::set_write_position(const size_t position) {
    if (position > data_.size()) {
        return false;
    }
    write_position_ = position;
    return true;
}

size_t rav::byte_stream::get_write_position() const {
    return write_position_;
}

void rav::byte_stream::flush() {}
