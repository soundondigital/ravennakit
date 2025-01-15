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

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/byte_order.hpp"

#include <cstddef>
#include <array>
#include <vector>

namespace rav {

/**
 * A class similar to std::string_view but for raw data buffers.
 * @tparam Type The data type.
 */
template<class Type>
class buffer_view {
  public:
    buffer_view() = default;

    buffer_view(const buffer_view& other) = default;
    buffer_view& operator=(const buffer_view& other) = default;

    buffer_view(buffer_view&& other) noexcept = default;
    buffer_view& operator=(buffer_view&& other) noexcept = default;

    /**
     * Construct a view pointing to given data.
     * @param data The data to refer to.
     * @param size The number of elements in the buffer.
     */
    buffer_view(Type* data, const size_t size) : data_(data), size_(size) {
        if (data_ == nullptr) {
            size_ = 0;
        }
    }

    /**
     * Construct a view from a std::array.
     * @tparam S The size of the array.
     * @param array The array to refer to.
     */
    template<size_t S>
    explicit buffer_view(const std::array<Type, S>& array) : buffer_view(array.data(), array.size()) {}

    /**
     * Construct a view from a std::vector.
     * @param vector The vector to refer to.
     */
    template<typename T = Type, std::enable_if_t<!std::is_const_v<T>, int> = 0>  // Type must be non-const for vector.
    explicit buffer_view(const std::vector<Type>& vector) : buffer_view(vector.data(), vector.size()) {}

    /**
     * @param index The index to access.
     * @returns Value for given index, without bounds checking.
     */
    Type operator[](size_t index) const {
        return data_[index];
    }

    /**
     * @returns A pointer to the data, or nullptr if this view is not pointing at any data.
     */
    [[nodiscard]] const Type* data() const {
        return data_;
    }

    /**
     * @returns A pointer to the data, or nullptr if this view is not pointing at any data.
     */
    [[nodiscard]] Type* data() {
        return data_;
    }

    /**
     * @returns The number of elements in the buffer.
     */
    [[nodiscard]] size_t size() const {
        return size_;
    }

    /**
     * @returns The size of the buffer in bytes.
     */
    [[nodiscard]] size_t size_bytes() const {
        return size_ * sizeof(Type);
    }

    /**
     * @returns True if the buffer is empty.
     */
    [[nodiscard]] bool empty() const {
        return size_ == 0;
    }

    /**
     * Reads a value from the data in native byte order (not to be confused with network-endian).
     * Bounds are asserted, and behaviour depends on the RAV_ASSERT configuration.
     * @tparam ValueType The type of the value to read.
     * @param offset The offset to read from.
     * @return The decoded value.
     */
    template<typename ValueType, std::enable_if_t<std::is_trivially_copyable_v<ValueType>, bool> = true>
    ValueType read_ne(const size_t offset) const {
        RAV_ASSERT(offset + sizeof(ValueType) <= size_bytes(), "Buffer view out of bounds");
        return byte_order::read_ne<ValueType>(data_ + offset);
    }

    /**
     * Reads a big-endian value from the given data.
     * @tparam ValueType The type of the value to read.
     * @param offset The offset to read from.
     * @return The decoded value.
     */
    template<typename ValueType, std::enable_if_t<std::is_trivially_copyable_v<ValueType>, bool> = true>
    ValueType read_be(const size_t offset) const {
        return byte_order::swap_if_le(read_ne<ValueType>(offset));
    }

    /**
     * Reads a little-endian value from the given data.
     * @tparam ValueType The type of the value to read.
     * @param offset The offset to read from.
     * @return The decoded value.
     */
    template<typename ValueType, std::enable_if_t<std::is_trivially_copyable_v<ValueType>, bool> = true>
    ValueType read_le(const size_t offset) const {
        return byte_order::swap_if_be(read_ne<ValueType>(offset));
    }

    /**
     * @returns A new buffer_view pointing to a sub-range of this buffer.
     * @param offset The offset of the sub-range.
     */
    [[nodiscard]] buffer_view subview(size_t offset) const {
        offset = std::min(offset, size_);
        return buffer_view(data_ + offset, size_ - offset);
    }

    /**
     * @returns A new buffer_view pointing to a sub-range of this buffer.
     * @param offset The offset of the sub-range.
     * @param size The number of elements in the sub-range. The size will be limited to the available size.
     */
    [[nodiscard]] buffer_view subview(size_t offset, const size_t size) const {
        offset = std::min(offset, size_);
        return buffer_view(data_ + offset, std::min(size_ - offset, size));
    }

    /**
     * Returns a new buffer_view pointing to the same data, but reinterpreted as a different type.
     * WARNING! Reinterpreting data can potentially lead to undefined behavior. Rules for reinterpret_cast apply.
     * @tparam NewType The type of the reinterpretation.
     * @return The new buffer_view.
     */
    template<class NewType>
    buffer_view<NewType> reinterpret() const {
        return buffer_view<NewType>(reinterpret_cast<NewType*>(data_), size_ * sizeof(Type) / sizeof(NewType));
    }

  private:
    Type* data_ {nullptr};
    size_t size_ {0};
};

}  // namespace rav
