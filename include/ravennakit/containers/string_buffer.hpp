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

#include "buffer_view.hpp"
#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/string_parser.hpp"

#include <string>

namespace rav {

/**
 * Contains a std::string and provides different facilities for working with it as a stream.
 */
class string_buffer {
  public:
    string_buffer() = default;

    /**
     * Constructs a string_stream with the given data.
     * @param data The initial data.
     */
    explicit string_buffer(std::string data) : data_(std::move(data)), write_position_(data_.size()) {}

    /**
     * Prepares space in the buffer for writing. The returned buffer_view is valid until the next call to prepare or
     * commit. The buffer will be resized if necessary to accommodate the requested size. After writing to the prepared
     * space, call commit to finalize the operation.
     * @param size The number of characters to reserve in the buffer.
     * @return A string_view representing the writable portion of the buffer.
     */
    buffer_view<std::string::value_type> prepare(const size_t size) {
        data_.resize(write_position_ + size);
        return {data_.data() + write_position_, size};
    }

    /**
     * Commits characters to the buffer, by moving the write position ahead.
     * @param size The amount of characters to commit.
     */
    void commit(const size_t size) {
        RAV_ASSERT(size <= data_.size() - write_position_, "Committing more data than prepared");
        write_position_ += size;
    }

    /**
     * @returns The number of characters available to read.
     */
    [[nodiscard]] size_t remaining() const {
        RAV_ASSERT(read_position_ <= write_position_, "Read position is greater than write position");
        return write_position_ - read_position_;
    }

    /**
     * @return True if there is no data available to read.
     */
    [[nodiscard]] bool exhausted() const {
        return read_position_ >= data_.size() || read_position_ == write_position_;
    }

    /**
     * @return A string_view to the data available to read.
     */
    [[nodiscard]] std::string_view data() const {
        return {data_.data() + read_position_, write_position_ - read_position_};
    }

    /**
     * Consumes characters from the buffer, by moving the read position ahead.
     * @param size The amount of characters to consume.
     */
    void consume(const size_t size) {
        RAV_ASSERT(size <= write_position_ - read_position_, "Consuming more data than available");
        read_position_ += size;
        if (read_position_ == write_position_) {
            read_position_ = 0;
            write_position_ = 0;
        }
    }

    /**
     * Reads data from the buffer, returning a view to the readable portion. The size of the returned view will be
     * either `max_size` or the amount of data available, whichever is smaller. The data is marked as consumed after
     * this operation. The returned view remains valid until either reset() is called or the buffer is reallocated.
     * @param max_size The maximum number of characters to read.
     * @return A string_view of the available data, up to `max_size`.
     */
    [[nodiscard]] std::string_view read(size_t max_size = std::numeric_limits<size_t>::max()) {
        RAV_ASSERT(read_position_ <= write_position_, "Read position is greater than write position");
        max_size = std::min(write_position_ - read_position_, max_size);
        const auto pos = read_position_;
        consume(max_size);
        return {data_.data() + pos, max_size};
    }

    /**
     * Reads until a newline is found. Newline can be \r\n or \n.
     * @returns The line until newline, or std::nullopt if no newline was found.
     */
    [[nodiscard]] std::optional<std::string_view> read_until_newline() {
        if (exhausted()) {
            return std::nullopt;
        }

        std::string_view view(data_.data() + read_position_, write_position_ - read_position_);

        const auto pos = view.find('\n');
        if (pos == std::string_view::npos) {
            return std::nullopt;
        }

        auto substr = view.substr(0, pos);
        consume(pos + 1);
        if (!substr.empty() && substr.back() == '\r') {
            substr.remove_suffix(1);  // Remove CR from CRLF
        }
        return substr;
    }

    /**
     * Tests if the next available data starts with the given prefix, without consuming any data.
     * @param prefix The prefix to test.
     * @return True if the next available data starts with the prefix.
     */
    [[nodiscard]] bool starts_with(const std::string_view prefix) const {
        return remaining() >= prefix.size()
            && std::equal(prefix.begin(), prefix.end(), data_.begin() + static_cast<long>(read_position_));
    }

    /**
     * Writes and commits data to the buffer.
     * @param data The data to write.
     */
    void write(const std::string_view& data) {
        auto buffer = prepare(data.size());
        std::memcpy(buffer.data(), data.data(), data.size());
        commit(data.size());
    }

    /**
     * Clears the data and sets the read and write positions to zero.
     */
    void clear() {
        data_.clear();
        read_position_ = 0;
        write_position_ = 0;
    }

  private:
    std::string data_;
    size_t read_position_ = 0;
    size_t write_position_ = 0;
};

}  // namespace rav
