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
#include <optional>
#include <string>

namespace rav {

/**
 * Baseclass for classes that want to provide stream-like access to data.
 */
class input_stream {
  public:
    enum class error {
        insufficient_data,
        failed_to_set_read_position,
    };

    input_stream() = default;
    virtual ~input_stream() = default;

    /**
     * Reads data from the stream into the given buffer.
     * If the stream doesn't have enough data, then nothing will be read.
     * @param buffer The buffer to read data into.
     * @param size The number of bytes to read.
     * @return The number of bytes read.
     */
    [[nodiscard]] virtual tl::expected<size_t, error> read(uint8_t* buffer, size_t size) = 0;

    /**
     * Sets the read position in the stream.
     * @param position The new read position.
     * @return True if the read position was successfully set.
     */
    [[nodiscard]] virtual bool set_read_position(size_t position) = 0;

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
    [[nodiscard]] virtual bool exhausted() = 0;

    /**
     * Skips size amount of bytes in the stream.
     * @param size The number of bytes to skip.
     * @return True if the skip was successful.
     */
    [[nodiscard]] bool skip(size_t size);

    /**
     * Reads size amount of bytes from the stream and returns it as a string.
     * Note: returned string might contain non-printable characters.
     * @param size The number of bytes to read.
     * @return The string read from the stream.
     */
    [[nodiscard]] tl::expected<std::string, error> read_as_string(size_t size);

    /**
     * Reads a value from the given stream in native byte order (not to be confused with network-endian).
     * @tparam Type The type of the value to read.
     * @return The decoded value.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    [[nodiscard]] tl::expected<Type, error> read_ne() {
        Type value;
        auto result = read(reinterpret_cast<uint8_t*>(std::addressof(value)), sizeof(Type));
        if (!result) {
            return tl::unexpected(result.error());
        }
        if (result.value() != sizeof(Type)) {
            return tl::unexpected(error::insufficient_data);
        }
        return value;
    }

    /**
     * Reads a big-endian value from the given stream.
     * @tparam Type The type of the value to read.
     * @return The decoded value.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    [[nodiscard]] tl::expected<Type, error> read_be() {
        return read_ne<Type>().map([](Type value) {
            return byte_order::swap_if_le(value);
        });
    }

    /**
     * Reads a little-endian value from the given stream.
     * @tparam Type The type of the value to read.
     * @return The decoded value.
     */
    template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
    [[nodiscard]] tl::expected<Type, error> read_le() {
        return read_ne<Type>().map([](Type value) {
            return byte_order::swap_if_be(value);
        });
    }

    /**
     * @param e The error to convert to a string.
     * @return The string representation of the error.
     */
    static const char* to_string(const error e) {
        switch (e) {
            case error::insufficient_data:
                return "insufficient data";
            case error::failed_to_set_read_position:
                return "failed to set read position";
        }
        return "unknown error";
    }
};

}  // namespace rav
