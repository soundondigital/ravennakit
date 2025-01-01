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

#include <vector>

namespace rav {

/**
 * A fixed size buffer that overwrites the oldest element when the buffer is full.
 * @tparam T The type of the elements stored in the buffer.
 */
template<class T>
class ring_buffer {
  public:
    explicit ring_buffer(size_t size) : data_(size) {
        RAV_ASSERT(size > 0, "Ring buffer must have a size greater than zero");
    }

    ring_buffer(std::initializer_list<T> initializer_list) : data_(initializer_list), count_(initializer_list.size()) {}

    ring_buffer(const ring_buffer& other) = default;
    ring_buffer& operator=(const ring_buffer& other) = default;

    ring_buffer(ring_buffer&& other) noexcept {
        if (this != &other) {
            std::swap(data_, other.data_);
            std::swap(read_index_, other.read_index_);
            std::swap(write_index_, other.write_index_);
            std::swap(count_, other.count_);
        }
    }

    ring_buffer& operator=(ring_buffer&& other) noexcept {
        if (this != &other) {
            std::swap(data_, other.data_);
            std::swap(read_index_, other.read_index_);
            std::swap(write_index_, other.write_index_);
            std::swap(count_, other.count_);
            other.clear();
        }
        return *this;
    }

    /**
     * Add an element to the buffer. If the buffer is full, the oldest element will be overwritten.
     * @param value The value to add to the buffer.
     */
    void push_back(const T& value) {
        data_[write_index_] = value;
        write_index_ = (write_index_ + 1) % data_.size();
        if (count_ < data_.size()) {
            ++count_;
        } else {
            read_index_ = (read_index_ + 1) % data_.size();
        }
    }

    /**
     * @return The oldest element in the buffer, or std::nullopt if the buffer is empty.
     */
    std::optional<T> pop_front() {
        if (empty()) {
            return std::nullopt;
        }
        T value = std::move(data_[read_index_]);
        read_index_ = (read_index_ + 1) % data_.size();
        --count_;
        return value;
    }

    /**
     * Indexing operator. Buffer must not have zero capacity.
     * @param index The logical index of the element to access. The index will wrap around if too high.
     * @return The element at the given index.
     */
    T& operator[](const size_t index) {
        return data_[(read_index_ + index) % data_.size()];
    }

    /**
     * Indexing operator. Buffer must not have zero capacity.
     * @param index The logical index of the element to access. The index will wrap around if too high.
     * @return The element at the given index.
     */
    const T& operator[](const size_t index) const {
        return data_[(read_index_ + index) % data_.size()];
    }

    /**
     * Get the size of the buffer. This is the number of elements currently stored in the buffer.
     * @return The size of the buffer.
     */
    [[nodiscard]] size_t size() const {
        return count_;
    }

    /**
     * @returns True if the buffer is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const {
        return count_ == 0;
    }

    /**
     * @returns True if the buffer is full, false otherwise.
     */
    [[nodiscard]] bool full() const {
        return count_ == data_.size();
    }

    /**
     * Clears the contents of the buffer.
     */
    void clear() {
        read_index_ = 0;
        write_index_ = 0;
        count_ = 0;
    }

    auto tie() {
        return std::tie(data_, read_index_, write_index_, count_);
    }

    friend bool operator==(const ring_buffer& lhs, const ring_buffer& rhs) {
        return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(const ring_buffer& lhs, const ring_buffer& rhs) {
        return lhs.tie() != rhs.tie();
    }

    /**
     * An iterator for the ring buffer.
     */
    template<typename BufferType, typename ValueType>
    class iterator_base {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = ValueType;
        using difference_type = std::ptrdiff_t;
        using pointer = ValueType*;
        using reference = ValueType&;

        iterator_base(BufferType& buffer, const size_t logical_index, const size_t remaining) :
            buffer_(buffer), logical_index_(logical_index), remaining_(remaining) {}

        reference operator*() {
            size_t physical_index = (buffer_.read_index_ + logical_index_) % buffer_.data_.size();
            return buffer_.data_[physical_index];
        }

        pointer operator->() {
            return &(**this);
        }

        iterator_base& operator++() {
            ++logical_index_;
            --remaining_;
            return *this;
        }

        iterator_base operator++(int) {
            iterator_base temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const iterator_base& other) const {
            return remaining_ == other.remaining_;
        }

        bool operator!=(const iterator_base& other) const {
            return !(*this == other);
        }

      private:
        BufferType& buffer_;
        size_t logical_index_;
        size_t remaining_;
    };

    using iterator = iterator_base<ring_buffer, T>;
    using const_iterator = iterator_base<const ring_buffer, const T>;

    iterator begin() {
        return iterator(*this, 0, count_);
    }

    iterator end() {
        return iterator(*this, count_, 0);
    }

    const_iterator begin() const {
        return const_iterator(*this, 0, count_);
    }

    const_iterator end() const {
        return const_iterator(*this, count_, 0);
    }

    const_iterator cbegin() const {
        return const_iterator(*this, 0, count_);
    }

    const_iterator cend() const {
        return const_iterator(*this, count_, 0);
    }

  private:
    std::vector<T> data_;
    size_t read_index_ = 0;
    size_t write_index_ = 0;
    size_t count_ = 0;
};

}  // namespace rav
