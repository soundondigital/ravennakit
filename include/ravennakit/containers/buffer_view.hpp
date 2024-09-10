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

#include <cstddef>

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
     * Constructs a view pointing to given data.
     * @param data The data to refer to.
     * @param count The number of elements in the buffer.
     */
    buffer_view(Type* data, const size_t count) : data_(data), count_(count) {
        if (data_ == nullptr) {
            count_ = 0;
        }
    }

    /**
     * @returns A pointer to the data, or nullptr if this view is not pointing at any data.
     */
    [[nodiscard]] const Type* data() const {
        return data_;
    }

    /**
     * @returns The number of elements in the buffer.
     */
    [[nodiscard]] size_t size() const {
        return count_;
    }

    /**
     * @returns The size of the buffer in bytes.
     */
    [[nodiscard]] size_t size_bytes() const {
        return count_ * sizeof(Type);
    }

    /**
     * @returns True if the buffer is empty.
     */
    [[nodiscard]] bool empty() const {
        return count_ == 0;
    }

  private:
    Type* data_ {nullptr};
    size_t count_ {0};
};

}  // namespace rsdk
