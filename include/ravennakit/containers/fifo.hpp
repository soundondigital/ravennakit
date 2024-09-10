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

#include <atomic>
#include <mutex>

namespace rav::fifo {

struct position {
    size_t index1 {};
    size_t size1 {};
    size_t size2 {};

    void update(const size_t pointer, const size_t capacity, const size_t number_of_elements) {
        index1 = pointer;
        size1 = number_of_elements;
        size2 = 0;

        if (pointer + number_of_elements > capacity) {
            size1 = capacity - pointer;
            size2 = number_of_elements - size1;
        }
    }
};

struct single {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(single* owner) : owner_(owner) {}

        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        single* owner_ {nullptr};
    };

    lock prepare_for_write(const size_t number_of_elements) {
        if (size_ + number_of_elements > capacity_) {
            return {};
        }

        lock write_lock(this);
        write_lock.position.update(tail_, capacity_, number_of_elements);
        return write_lock;
    }

    lock prepare_for_read(const size_t number_of_elements) {
        if (size_ < number_of_elements) {
            return {};
        }

        lock read_lock(this);
        read_lock.position.update(head_, capacity_, number_of_elements);
        return read_lock;
    }

    [[nodiscard]] size_t size() const {
        return size_;
    }

    void commit_write(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        tail_ = (tail_ + size) % capacity_;
        size_ += size;
    }

    void commit_read(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        head_ = (head_ + size) % capacity_;
        size_ -= size;
    }

    void resize(const size_t num_elements) {
        reset();
        capacity_ = num_elements;
    }

    void reset() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

  private:
    size_t head_ = 0;  // Consumer index
    size_t tail_ = 0;  // Producer index
    size_t capacity_ = 0;
    size_t size_ = 0;
};

struct spsc {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(spsc* owner) : owner_(owner) {}

        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        spsc* owner_ {nullptr};
    };

    lock prepare_for_write(const size_t number_of_elements) {
        if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
            return {};  // Not enough free space in buffer.
        }

        lock write_lock(this);
        write_lock.position.update(tail_, capacity_, number_of_elements);
        return write_lock;
    }

    lock prepare_for_read(const size_t number_of_elements) {
        if (size_.load(std::memory_order_acquire) < number_of_elements) {
            return {};  // Not enough data available.
        }

        lock read_lock(this);
        read_lock.position.update(head_, capacity_, number_of_elements);
        return read_lock;
    }

    void commit_write(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        tail_ = (tail_ + size) % capacity_;
        size_.fetch_add(size, std::memory_order_release);
    }

    void commit_read(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        head_ = (head_ + size) % capacity_;
        size_.fetch_sub(size, std::memory_order_release);
    }

    void resize(const size_t num_elements) {
        reset();
        capacity_ = num_elements;
    }

    void reset() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

  private:
    size_t head_ = 0;               // Consumer index
    size_t tail_ = 0;               // Producer index
    std::atomic<size_t> size_ = 0;  // Number of elements in the buffer
    size_t capacity_ = 0;
};

struct mpsc {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(mpsc* owner) : owner_(owner) {}

        explicit lock(mpsc* owner, std::unique_lock<std::mutex>&& lock) : owner_(owner), lock_(std::move(lock)) {}

        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        mpsc* owner_ {nullptr};
        std::unique_lock<std::mutex> lock_;
    };

    lock prepare_for_write(const size_t number_of_elements) {
        std::unique_lock guard(mutex_);

        if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
            return {};  // Not enough free space in buffer.
        }

        lock write_lock(this, std::move(guard));
        write_lock.position.update(tail_, capacity_, number_of_elements);
        return write_lock;
    }

    lock prepare_for_read(const size_t number_of_elements) {
        if (size_.load(std::memory_order_acquire) < number_of_elements) {
            return {};  // Not enough data available.
        }

        lock read_lock(this);
        read_lock.position.update(head_, capacity_, number_of_elements);
        return read_lock;
    }

    void commit_write(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        tail_ = (tail_ + size) % capacity_;
        size_.fetch_add(size, std::memory_order_release);
    }

    void commit_read(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        head_ = (head_ + size) % capacity_;
        size_.fetch_sub(size, std::memory_order_release);
    }

    void resize(const size_t num_elements) {
        reset();
        capacity_ = num_elements;
    }

    void reset() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

  private:
    size_t head_ = 0;  // Consumer index
    size_t tail_ = 0;  // Producer index
    size_t capacity_ = 0;
    std::atomic<size_t> size_ = 0;  // Number of elements in the buffer
    std::mutex mutex_;
};

struct spmc {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(spmc* owner) : owner_(owner) {}

        explicit lock(spmc* owner, std::unique_lock<std::mutex>&& lock) : owner_(owner), lock_(std::move(lock)) {}

        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        spmc* owner_ {nullptr};
        std::unique_lock<std::mutex> lock_;
    };

    lock prepare_for_write(const size_t number_of_elements) {
        if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
            return {};  // Not enough free space in buffer.
        }

        lock write_lock(this);
        write_lock.position.update(tail_, capacity_, number_of_elements);
        return write_lock;
    }

    lock prepare_for_read(const size_t number_of_elements) {
        std::unique_lock guard(mutex_);

        if (size_.load(std::memory_order_acquire) < number_of_elements) {
            return {};  // Not enough data available.
        }

        lock read_lock(this, std::move(guard));
        read_lock.position.update(head_, capacity_, number_of_elements);
        return read_lock;
    }

    void commit_write(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        tail_ = (tail_ + size) % capacity_;
        size_.fetch_add(size, std::memory_order_release);
    }

    void commit_read(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        head_ = (head_ + size) % capacity_;
        size_.fetch_sub(size, std::memory_order_release);
    }

    void resize(const size_t num_elements) {
        reset();
        capacity_ = num_elements;
    }

    void reset() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

  private:
    size_t head_ = 0;  // Consumer index
    size_t tail_ = 0;  // Producer index
    size_t capacity_ = 0;
    std::atomic<size_t> size_ = 0;  // Number of elements in the buffer
    std::mutex mutex_;
};

struct mpmc {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(mpmc* owner) : owner_(owner) {}

        explicit lock(mpmc* owner, std::unique_lock<std::mutex>&& lock) : owner_(owner), lock_(std::move(lock)) {}

        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        mpmc* owner_ {nullptr};
        std::unique_lock<std::mutex> lock_;
    };

    lock prepare_for_write(const size_t number_of_elements) {
        std::unique_lock guard(mutex_);

        if (size_ + number_of_elements > capacity_) {
            return {};  // Not enough free space in buffer.
        }

        lock write_lock(this, std::move(guard));
        write_lock.position.update(tail_, capacity_, number_of_elements);
        return write_lock;
    }

    lock prepare_for_read(const size_t number_of_elements) {
        std::unique_lock guard(mutex_);

        if (size_ < number_of_elements) {
            return {};  // Not enough data available.
        }

        lock read_lock(this, std::move(guard));
        read_lock.position.update(head_, capacity_, number_of_elements);
        return read_lock;
    }

    void commit_write(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        tail_ = (tail_ + size) % capacity_;
        size_ += size;
    }

    void commit_read(const lock& lock) {
        const auto size = lock.position.size1 + lock.position.size2;
        head_ = (head_ + size) % capacity_;
        size_ -= size;
    }

    void resize(const size_t num_elements) {
        reset();
        capacity_ = num_elements;
    }

    void reset() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

  private:
    size_t head_ = 0;  // Consumer index
    size_t tail_ = 0;  // Producer index
    size_t capacity_ = 0;
    size_t size_ = 0;  // Number of elements in the buffer
    std::mutex mutex_;
};

}  // namespace rav::fifo
