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
#include "ravennakit/core/expected.hpp"

#include <cstdint>

namespace rav {

/**
 * An abstract class representing an output stream, providing all kinds of write operations.
 */
class output_stream {
  public:
    enum class error {
        failed_to_write,
        out_of_memory,
    };

    output_stream() = default;
    virtual ~output_stream() = default;

    /**
     * Writes data from the given buffer to the stream.
     * @param buffer The buffer to write data from.
     * @param size The number of bytes to write.
     * @return An expected indicating success or failure.
     */
    [[nodiscard]] virtual tl::expected<void, error> write(const uint8_t* buffer, size_t size) = 0;

    /**
     * Convenience function to write data from a char buffer to the stream.
     * @param buffer The buffer to write data from.
     * @param size The number of bytes to write.
     * @return An expected indicating success or failure.
     */
    [[nodiscard]] tl::expected<void, error> write(const char* buffer, const size_t size) {
        return write(reinterpret_cast<const uint8_t*>(buffer), size);
    }

    /**
     * Sets the write position in the stream.
     * @param position The new write position.
     * @return True if the write position was successfully set.
     */
    [[nodiscard]] virtual tl::expected<void, error> set_write_position(size_t position) = 0;

    /**
     * @return The current write position in the stream.
     */
    [[nodiscard]] virtual size_t get_write_position() = 0;

    /**
     * Flushes the stream, ensuring that all data is written to the underlying storage. Not all streams support this
     * operation.
     */
    virtual void flush() = 0;

    /**
     * Writes a value to the stream in native byte order (not to be confused with network-endian).
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     * @return An expected indicating success or failure.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    [[nodiscard]] tl::expected<void, error> write_ne(const Type value) {
        return write(reinterpret_cast<const uint8_t*>(std::addressof(value)), sizeof(Type));
    }

    /**
     * Writes a big-endian value to the stream.
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     * @return An expected indicating success or failure.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    [[nodiscard]] tl::expected<void, error> write_be(const Type value) {
        return write_ne(byte_order::swap_if_le(value));
    }

    /**
     * Writes a little-endian value to the stream.
     * @tparam Type The type of the value to write.
     * @param value The value to write.
     * @return An expected indicating success or failure.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    [[nodiscard]] tl::expected<void, error> write_le(const Type value) {
        return write_ne(byte_order::swap_if_be(value));
    }

    /**
     * Writes a string to the stream. The string is prefixed with its size in little-endian format.
     * @param str The string to write.
     * @return An expected indicating success or failure.
     */
    [[nodiscard]] tl::expected<void, error> write_string(const std::string& str) {
        OK_OR_RETURN(write_le<uint64_t>(str.size()));
        OK_OR_RETURN(write(reinterpret_cast<const uint8_t*>(str.data()), str.size()));
        return {};
    }

    /**
     * Writes a c-string to the stream, up to and including the null character.
     * If the string doesn't have a null character then the behaviour is undefined (and probably will lead to invalid
     * memory access).
     * @param str The string to write.
     * @return An expected indicating success or failure.
     */
    [[nodiscard]] tl::expected<void, error> write_cstring(const char* str) {
        return write(reinterpret_cast<const uint8_t*>(str), std::strlen(str) + 1);
    }
};

}  // namespace rav
