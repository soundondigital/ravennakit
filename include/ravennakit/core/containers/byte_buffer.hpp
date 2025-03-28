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

#include "ravennakit/core/byte_order.hpp"

#include <vector>

namespace rav {

/**
 * A wrapper around std::vector with some facilities for writing different types to it in different byte orders.
 */
class ByteBuffer {
  public:
    ByteBuffer() = default;
    explicit ByteBuffer(const size_t size) : data_(size) {}

    ByteBuffer(const ByteBuffer&) = default;
    ByteBuffer& operator=(const ByteBuffer&) = default;

    ByteBuffer(ByteBuffer&&) noexcept = default;
    ByteBuffer& operator=(ByteBuffer&&) noexcept = default;

    /**
     * @return A pointer to the data in the buffer.
     */
    [[nodiscard]] const uint8_t* data() const {
        return data_.data();
    }

    /**
     * @return The current size of the buffer.
     */
    [[nodiscard]] size_t size() const {
        return data_.size();
    }

    /**
     * Clears the data.
     */
    void clear() {
        data_.clear();
    }

    /**
     * Appends given data to the buffer.
     * @param data The data to append.
     * @param size The size of the data.
     */
    void write(const uint8_t* data, const size_t size) {
        data_.insert(data_.end(), data, data + size);
    }

    /**
     * Writes a value to the stream in native byte order (not to be confused with network-endian).
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     * @return An expected indicating success or failure.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    void write_ne(const Type value) {
        write(reinterpret_cast<const uint8_t*>(std::addressof(value)), sizeof(Type));
    }

    /**
     * Writes a big-endian value to the stream.
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     * @return An expected indicating success or failure.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    void write_be(const Type value) {
        write_ne(swap_if_le(value));
    }

    /**
     * Writes a little-endian value to the stream.
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     * @return An expected indicating success or failure.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    void write_le(const Type value) {
        return write_ne(swap_if_be(value));
    }

  private:
    std::vector<uint8_t> data_;
};

}  // namespace rav
