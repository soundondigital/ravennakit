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

#include "ravennakit/core/byte_order.hpp"

#include <cstdint>
#include <limits>

namespace rav {

/**
 * An abstract class representing an output stream, providing all kinds of write operations.
 */
class output_stream {
  public:
    output_stream() = default;
    virtual ~output_stream() = default;

    /**
     * Writes data from the given buffer to the stream.
     * @param buffer The buffer to write data from.
     * @param size The number of bytes to write.
     * @return The number of bytes written.
     */
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;

    /**
     * Sets the write position in the stream.
     * @param position The new write position.
     * @return True if the write position was successfully set.
     */
    virtual bool set_write_position(size_t position) = 0;

    /**
     * @return The current write position in the stream.
     */
    [[nodiscard]] virtual size_t get_write_position() const = 0;

    /**
     * Flushes the stream, ensuring that all data is written to the underlying storage. Not all streams support this
     * operation.
     */
    virtual void flush() = 0;

    /**
     * Writes a value to the stream in native byte order (not to be confused with network-endian).
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    size_t write_ne(const Type value) {
        return write(reinterpret_cast<const uint8_t*>(std::addressof(value)), sizeof(Type));
    }

    /**
     * Writes a big-endian value to the stream.
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    void write_be(const Type value) {
        write_ne(swap_if_le(value));
    }

    /**
     * Writes a little-endian value to the stream.
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    void write_le(const Type value) {
        write_ne(byte_order::swap_if_be(value));
    }

    /**
     * Writes a string to the stream.
     * @param str The string to write.
     */
    size_t write_string(const std::string& str) {
        return write(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }

    /**
     * Writes a c-string to the stream, up to and including the null character.
     * If the string doesn't have a null character, and max_size is bigger than the amount of characters then the
     * behaviour is undefined (and probably will lead to invalid memory access).
     * @param str The string to write.
     * @param max_size The max size to write.
     */
    size_t write_cstring(const char* str, const size_t max_size = std::numeric_limits<size_t>::max()) {
        return write(reinterpret_cast<const uint8_t*>(str), std::min(std::strlen(str) + 1, max_size));
    }
};

}  // namespace rav
