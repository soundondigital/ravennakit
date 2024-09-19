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

#include <cstdint>
#include <cstring>
#include <vector>

#include "ravennakit/platform/byte_order.hpp"

namespace rav {

/**
 * Simple stream implementation that writes to and reads from a vector.
 */
template<class T>
class vector_stream {
  public:
    vector_stream() = default;
    explicit vector_stream(size_t size) : data_(size) {}
    vector_stream(std::initializer_list<T> values) : data_(values) {}

    vector_stream(const vector_stream& other) = default;
    vector_stream(vector_stream&& other) noexcept = default;
    vector_stream& operator=(const vector_stream& other) = default;
    vector_stream& operator=(vector_stream&& other) noexcept = default;

    bool operator==(const std::vector<T>& other) const {
        return data_ == other;
    }

    /**
     * Writes the given value to the stream in native byte order.
     * @param value The value to write.
     */
    void push_back(T value) {
        data_.push_back(value);
    }

    /**
     * Writes the given values to the stream in native byte order.
     * @param values The values to write.
     */
    void push_back(std::initializer_list<T> values) {
        for (const auto& value : values) {
            push_back(value);
        }
    }

    /**
     * Writes the given value to the stream in big endian byte order.
     * @tparam T The type of the value to write.
     * @param value The value to write.
     */
    void push_back_be(T value) {
        if constexpr (little_endian) {
            push_back(byte_order::swap_bytes(value));
        } else {
            push_back(value);
        }
    }

    /**
     * Writes the given value to the stream in big endian byte order.
     * @tparam T The type of the value to write.
     * @param values The values to write.
     */
    void push_back_be(std::initializer_list<T> values) {
        for (const auto& value : values) {
            push_back_be(value);
        }
    }

    /**
     * Writes the given value to the stream in little endian byte order.
     * @tparam T The type of the value to write.
     * @param value The value to write.
     * @return True if the value was written successfully, false otherwise.
     */
    void push_back_le(T value) {
        if constexpr (big_endian) {
            push_back(byte_order::swap_bytes(value));
        } else {
            push_back(value);
        }
    }

    /**
     * Writes the given value to the stream in little endian byte order.
     * @tparam T The type of the value to write.
     * @param values The values to write.
     */
    void push_back_le(std::initializer_list<T> values) {
        for (const auto& value : values) {
            push_back_le(value);
        }
    }

    /**
     * Reads a value from the stream in native byte order.
     * @tparam T The type of the value to read.
     * @return The value read from the stream, or a default-constructed value if the read failed.
     */
    T read() {
        if (read_position_ >= data_.size()) {
            return T {};
        }
        return data_[read_position_++];
    }

    /**
     * Reads a value from the stream in big endian byte order.
     * @tparam T The type of the value to read.
     * @return The value read from the stream, or a default-constructed value if the read failed.
     */
    T read_be() {
        if constexpr (little_endian) {
            return byte_order::swap_bytes(read());
        }
        return read();
    }

    /**
     * Reads a value from the stream in little endian byte order.
     * @tparam T The type of the value to read.
     * @return The value read from the stream, or a default-constructed value if the read failed.
     */
    T read_le() {
        if constexpr (big_endian) {
            return byte_order::swap_bytes(read());
        }
        return read();
    }

    /**
     * @return Returns a pointer to the data in the stream.
     */
    [[nodiscard]] const T* data() const {
        return data_.data();
    }

    /**
     * @return Returns a pointer to the data in the stream.
     */
    [[nodiscard]] T* data() {
        return data_.data();
    }

    /**
     * @return Returns the size of the data in the stream.
     */
    [[nodiscard]] size_t size() const {
        return data_.size();
    }

    /**
     * Resizes the stream to the given size.
     * @param size The new size of the stream.
     */
    void resize(size_t size) {
        data_.resize(size);
    }

    /**
     * Resets the stream to its initial state.
     */
    void reset() {
        read_position_ = 0;
        data_.clear();
    }

  private:
    std::vector<T> data_;
    size_t read_position_ = 0;
};

}  // namespace rav
