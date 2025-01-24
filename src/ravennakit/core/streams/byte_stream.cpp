/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/streams/byte_stream.hpp"

rav::byte_stream::byte_stream(std::vector<uint8_t> data) : data_(std::move(data)), write_position_(data_.size()) {}

void rav::byte_stream::reset() {
    data_.clear();
    read_position_ = 0;
    write_position_ = 0;
}

tl::expected<size_t, rav::input_stream::error> rav::byte_stream::read(uint8_t* buffer, const size_t size) {
    if (data_.size() - read_position_ < size) {
        return tl::unexpected(input_stream::error::insufficient_data);
    }
    std::memcpy(buffer, data_.data() + read_position_, size);
    read_position_ += size;
    return size;
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

bool rav::byte_stream::exhausted() {
    return read_position_ >= data_.size();
}

tl::expected<void, rav::output_stream::error> rav::byte_stream::write(const uint8_t* buffer, const size_t size) {
    try {
        if (write_position_ + size > data_.size()) {
            data_.resize(write_position_ + size, 0);
        }
    } catch (...) {
        return tl::unexpected(output_stream::error::out_of_memory);
    }

    std::memcpy(data_.data() + write_position_, buffer, size);
    write_position_ += size;
    return {};
}

tl::expected<void, rav::output_stream::error> rav::byte_stream::set_write_position(const size_t position) {
    write_position_ = position;
    return {};
}

size_t rav::byte_stream::get_write_position() {
    return write_position_;
}

void rav::byte_stream::flush() {}
