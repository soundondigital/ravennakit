/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <ravennakit/containers/detail/fifo.hpp>

void rav::fifo::position::update(const size_t pointer, const size_t capacity, const size_t number_of_elements) {
    index1 = pointer;
    size1 = number_of_elements;
    size2 = 0;

    if (pointer + number_of_elements > capacity) {
        size1 = capacity - pointer;
        size2 = number_of_elements - size1;
    }
}

rav::fifo::single::lock rav::fifo::single::prepare_for_write(const size_t number_of_elements) {
    if (size_ + number_of_elements > capacity_) {
        return {};
    }

    lock write_lock(this);
    write_lock.position.update(tail_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::single::lock rav::fifo::single::prepare_for_read(const size_t number_of_elements) {
    if (size_ < number_of_elements) {
        return {};
    }

    lock read_lock(this);
    read_lock.position.update(head_, capacity_, number_of_elements);
    return read_lock;
}

size_t rav::fifo::single::size() const {
    return size_;
}

void rav::fifo::single::commit_write(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    tail_ = (tail_ + size) % capacity_;
    size_ += size;
}

void rav::fifo::single::commit_read(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    head_ = (head_ + size) % capacity_;
    size_ -= size;
}

void rav::fifo::single::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::single::reset() {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
}

rav::fifo::spsc::lock rav::fifo::spsc::prepare_for_write(const size_t number_of_elements) {
    if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
        return {};  // Not enough free space in buffer.
    }

    lock write_lock(this);
    write_lock.position.update(tail_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::spsc::lock rav::fifo::spsc::prepare_for_read(const size_t number_of_elements) {
    if (size_.load(std::memory_order_acquire) < number_of_elements) {
        return {};  // Not enough data available.
    }

    lock read_lock(this);
    read_lock.position.update(head_, capacity_, number_of_elements);
    return read_lock;
}

void rav::fifo::spsc::commit_write(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    tail_ = (tail_ + size) % capacity_;
    size_.fetch_add(size, std::memory_order_release);
}

void rav::fifo::spsc::commit_read(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    head_ = (head_ + size) % capacity_;
    size_.fetch_sub(size, std::memory_order_release);
}

void rav::fifo::spsc::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::spsc::reset() {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
}

rav::fifo::mpsc::lock rav::fifo::mpsc::prepare_for_write(const size_t number_of_elements) {
    std::unique_lock guard(mutex_);

    if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
        return {};  // Not enough free space in buffer.
    }

    lock write_lock(this, std::move(guard));
    write_lock.position.update(tail_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::mpsc::lock rav::fifo::mpsc::prepare_for_read(const size_t number_of_elements) {
    if (size_.load(std::memory_order_acquire) < number_of_elements) {
        return {};  // Not enough data available.
    }

    lock read_lock(this);
    read_lock.position.update(head_, capacity_, number_of_elements);
    return read_lock;
}

void rav::fifo::mpsc::commit_write(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    tail_ = (tail_ + size) % capacity_;
    size_.fetch_add(size, std::memory_order_release);
}

void rav::fifo::mpsc::commit_read(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    head_ = (head_ + size) % capacity_;
    size_.fetch_sub(size, std::memory_order_release);
}

void rav::fifo::mpsc::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::mpsc::reset() {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
}

rav::fifo::spmc::lock rav::fifo::spmc::prepare_for_write(const size_t number_of_elements) {
    if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
        return {};  // Not enough free space in buffer.
    }

    lock write_lock(this);
    write_lock.position.update(tail_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::spmc::lock rav::fifo::spmc::prepare_for_read(const size_t number_of_elements) {
    std::unique_lock guard(mutex_);

    if (size_.load(std::memory_order_acquire) < number_of_elements) {
        return {};  // Not enough data available.
    }

    lock read_lock(this, std::move(guard));
    read_lock.position.update(head_, capacity_, number_of_elements);
    return read_lock;
}

void rav::fifo::spmc::commit_write(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    tail_ = (tail_ + size) % capacity_;
    size_.fetch_add(size, std::memory_order_release);
}

void rav::fifo::spmc::commit_read(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    head_ = (head_ + size) % capacity_;
    size_.fetch_sub(size, std::memory_order_release);
}

void rav::fifo::spmc::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::spmc::reset() {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
}

rav::fifo::mpmc::lock rav::fifo::mpmc::prepare_for_write(const size_t number_of_elements) {
    std::unique_lock guard(mutex_);

    if (size_ + number_of_elements > capacity_) {
        return {};  // Not enough free space in buffer.
    }

    lock write_lock(this, std::move(guard));
    write_lock.position.update(tail_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::mpmc::lock rav::fifo::mpmc::prepare_for_read(const size_t number_of_elements) {
    std::unique_lock guard(mutex_);

    if (size_ < number_of_elements) {
        return {};  // Not enough data available.
    }

    lock read_lock(this, std::move(guard));
    read_lock.position.update(head_, capacity_, number_of_elements);
    return read_lock;
}

void rav::fifo::mpmc::commit_write(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    tail_ = (tail_ + size) % capacity_;
    size_ += size;
}

void rav::fifo::mpmc::commit_read(const lock& lock) {
    const auto size = lock.position.size1 + lock.position.size2;
    head_ = (head_ + size) % capacity_;
    size_ -= size;
}

void rav::fifo::mpmc::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::mpmc::reset() {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
}
