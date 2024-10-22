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
#include <optional>
#include <string>

namespace rav {

/**
 * Baseclass for classes that want to provide stream-like access to data.
 */
class input_stream {
  public:
    input_stream() = default;
    virtual ~input_stream() = default;

    /**
     * Reads data from the stream into the given buffer.
     * @param buffer The buffer to read data into.
     * @param size The number of bytes to read.
     * @return The number of bytes read.
     */
    virtual size_t read(uint8_t* buffer, size_t size) = 0;

    /**
     * Sets the read position in the stream.
     * @param position The new read position.
     * @return True if the read position was successfully set.
     */
    virtual bool set_read_position(size_t position) = 0;

    /**
     * @return The current read position in the stream.
     */
    [[nodiscard]] virtual size_t get_read_position() = 0;

    /**
     * @return The total number of bytes in this stream. Not all streams support this operation, in which case an empty
     * optional is returned.
     */
    [[nodiscard]] virtual std::optional<size_t> size() const = 0;

    /**
     * @return The number of bytes remaining to read in this stream. Not all streams support this operation, in which
     * case an empty optional is returned.
     */
    [[nodiscard]] std::optional<size_t> remaining();

    /**
     * @return True if the stream has no more data to read.
     */
    [[nodiscard]] virtual bool exhausted() const = 0;

    /**
     * Skips size amount of bytes in the stream.
     * @param size The number of bytes to skip.
     * @return True if the skip was successful.
     */
    bool skip(const size_t size);

    /**
     * Reads size amount of bytes from the stream and returns it as a string.
     * Note: returned string might contain non-printable characters.
     * @param size The number of bytes to read.
     * @return The string read from the stream.
     */
    std::string read_as_string(size_t size);

    /**
     * Reads a value from the given stream in native byte order (not to be confused with network-endian).
     * @tparam Type The type of the value to read.
     * @return The decoded value.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    std::optional<Type> read_ne() {
        Type value;
        const auto n = read(reinterpret_cast<uint8_t*>(std::addressof(value)), sizeof(Type));
        if (n != sizeof(Type)) {
            return std::nullopt;
        }
        return value;
    }

    /**
     * Reads a big-endian value from the given stream.
     * @tparam Type The type of the value to read.
     * @return The decoded value.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    std::optional<Type> read_be() {
        if (auto value = read_ne<Type>()) {
            return byte_order::swap_if_le(*value);
        }
        return std::nullopt;
    }

    /**
     * Reads a little-endian value from the given stream.
     * @tparam Type The type of the value to read.
     * @return The decoded value.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    std::optional<Type> read_le() {
        if (auto value = read_ne<Type>()) {
            return byte_order::swap_if_be(*value);
        }
        return std::nullopt;
    }
};

}  // namespace rav
