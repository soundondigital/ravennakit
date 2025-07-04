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

#include "detail/fifo.hpp"

#include <vector>
#include <cstring>
#include <optional>

namespace rav {

/**
 * A classic FIFO buffer implementation with different strategies F.
 */
template<class T, class F>
class FifoBuffer {
  public:
    FifoBuffer() = default;

    /**
     * Constructs a queue with a given number of elements.
     * @param capacity The number of elements the queue can hold.
     */
    explicit FifoBuffer(const size_t capacity) {
        resize(capacity);
    }

    FifoBuffer(const FifoBuffer& other) = delete;
    FifoBuffer& operator=(const FifoBuffer& other) = delete;

    FifoBuffer(FifoBuffer&& other) noexcept = delete;
    FifoBuffer& operator=(FifoBuffer&& other) noexcept = delete;

    /**
     * Pushes a value to the buffer.
     * @param value The value to push.
     * @return True if the value was pushed, false if the buffer is full.
     */
    [[nodiscard]] bool push(T value) {
        if (auto lock = fifo_.prepare_for_write(1)) {
            lock.position.size1 > 0 ? buffer_[lock.position.index1] = std::move(value) : buffer_[0] = std::move(value);
            lock.commit();
            return true;
        }
        return false;
    }

    /**
     * Pops a value from the buffer.
     * @return The value that was popped, or std::nullopt if the buffer is empty.
     */
    [[nodiscard]] std::optional<T> pop() {
        if (auto lock = fifo_.prepare_for_read(1)) {
            T value;
            lock.position.size1 > 0 ? value = std::move(buffer_[lock.position.index1]) : value = std::move(buffer_[0]);
            lock.commit();
            return value;
        }
        return std::nullopt;
    }

    /**
     * Convenience function to pop all available data. Thread safe when called from the consumer thread.
     */
    void pop_all() {
        if (auto lock = fifo_.prepare_for_read(size())) {
            lock.commit();
        }
    }

    /**
     * Writes data to the buffer.
     * @param src The source data to write to the buffer.
     * @param number_of_elements The number of elements to write (not bytes).
     * @return True if writing was successful, false if there was not enough space to write all data.
     */
    template<typename U = T, std::enable_if_t<std::is_trivially_copyable_v<U>, int> = 0>
    [[nodiscard]] bool write(const T* src, const size_t number_of_elements) {
        if (auto lock = fifo_.prepare_for_write(number_of_elements)) {
            std::memcpy(buffer_.data() + lock.position.index1, src, lock.position.size1 * sizeof(T));

            if (lock.position.size2 > 0) {
                std::memcpy(buffer_.data(), src + lock.position.size1, lock.position.size2 * sizeof(T));
            }

            lock.commit();

            return true;
        }

        return false;
    }

    /**
     * Reads data from the buffer.
     * @param dst The destination to write the data to.
     * @param number_of_elements The number of elements to read (not bytes).
     * @return True if reading was successful, false if there was not enough data to read.
     */
    template<typename U = T, std::enable_if_t<std::is_trivially_copyable_v<U>, int> = 0>
    [[nodiscard]] bool read(T* dst, const size_t number_of_elements) {
        if (auto lock = fifo_.prepare_for_read(number_of_elements)) {
            std::memcpy(dst, buffer_.data() + lock.position.index1, lock.position.size1 * sizeof(T));

            if (lock.position.size2 > 0) {
                std::memcpy(dst + lock.position.size1, buffer_.data(), lock.position.size2 * sizeof(T));
            }

            lock.commit();

            return true;
        }

        return false;
    }

    /**
     * Resizes this buffer, clearing existing data.
     * @param size The new capacity of the buffer.
     */
    void resize(size_t size) {
        buffer_.resize(size);
        fifo_.resize(size);
    }

    /**
     * Clears the buffer.
     */
    void reset() {
        buffer_.clear();
        fifo_.reset();
    }

    /**
     * @returns The number of elements in the buffer.
     */
    [[nodiscard]] size_t size() {
        return fifo_.size();
    }

  private:
    std::vector<T> buffer_;
    F fifo_;
};

}  // namespace rav
