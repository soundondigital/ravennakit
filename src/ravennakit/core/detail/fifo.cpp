/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/containers/detail/fifo.hpp"

rav::fifo::position::position(const size_t timestamp, const size_t capacity, const size_t number_of_elements) {
    update(timestamp, capacity, number_of_elements);
}

void rav::fifo::position::update(const size_t timestamp, const size_t capacity, const size_t number_of_elements) {
    RAV_ASSERT(number_of_elements <= capacity, "Number of elements must be less than or equal to capacity.");
    index1 = timestamp % capacity;
    size1 = number_of_elements;
    size2 = 0;

    if (index1 + number_of_elements > capacity) {
        size1 = capacity - index1;
        size2 = number_of_elements - size1;
    }
}

size_t rav::fifo::spmc::size() const {
    return size_;
}

rav::fifo::single::lock rav::fifo::single::prepare_for_write(const size_t number_of_elements) {
    if (size() + number_of_elements > capacity_) {
        return {};
    }

    lock write_lock([this, number_of_elements] {
        write_ts_ += number_of_elements;
    });
    write_lock.position.update(write_ts_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::single::lock rav::fifo::single::prepare_for_read(const size_t number_of_elements) {
    if (size() < number_of_elements) {
        return {};
    }

    lock read_lock([this, number_of_elements] {
        read_ts_ += number_of_elements;
    });
    read_lock.position.update(read_ts_, capacity_, number_of_elements);
    return read_lock;
}

size_t rav::fifo::single::size() const {
    return write_ts_ - read_ts_;
}

void rav::fifo::single::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::single::reset() {
    read_ts_ = 0;
    write_ts_ = 0;
}

rav::fifo::spsc::lock rav::fifo::spsc::prepare_for_write(const size_t number_of_elements) {
    if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
        return {};  // Not enough free space in buffer.
    }

    lock write_lock([this, number_of_elements] {
        write_ts_ += number_of_elements;
        size_.fetch_add(number_of_elements, std::memory_order_release);
    });
    write_lock.position.update(write_ts_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::spsc::lock rav::fifo::spsc::prepare_for_read(const size_t number_of_elements) {
    if (size_.load(std::memory_order_acquire) < number_of_elements) {
        return {};  // Not enough data available.
    }

    lock read_lock([this, number_of_elements] {
        read_ts_ += number_of_elements;
        size_.fetch_sub(number_of_elements, std::memory_order_release);
    });
    read_lock.position.update(read_ts_, capacity_, number_of_elements);
    return read_lock;
}

size_t rav::fifo::spsc::size() const {
    return size_;
}

void rav::fifo::spsc::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::spsc::reset() {
    read_ts_ = 0;
    write_ts_ = 0;
    size_ = 0;
}

rav::fifo::mpsc::lock rav::fifo::mpsc::prepare_for_write(const size_t number_of_elements) {
    std::unique_lock guard(mutex_);

    if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
        return {};  // Not enough free space in buffer.
    }

    lock write_lock(
        [this, number_of_elements] {
            write_ts_ += number_of_elements;
            size_.fetch_add(number_of_elements, std::memory_order_release);
        },
        std::move(guard)
    );
    write_lock.position.update(write_ts_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::mpsc::lock rav::fifo::mpsc::prepare_for_read(const size_t number_of_elements) {
    if (size_.load(std::memory_order_acquire) < number_of_elements) {
        return {};  // Not enough data available.
    }

    lock read_lock([this, number_of_elements] {
        read_ts_ += number_of_elements;
        size_.fetch_sub(number_of_elements, std::memory_order_release);
    });
    read_lock.position.update(read_ts_, capacity_, number_of_elements);
    return read_lock;
}

size_t rav::fifo::mpsc::size() const {
    return size_;
}

void rav::fifo::mpsc::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::mpsc::reset() {
    read_ts_ = 0;
    write_ts_ = 0;
    size_ = 0;
}

rav::fifo::spmc::lock rav::fifo::spmc::prepare_for_write(const size_t number_of_elements) {
    if (size_.load(std::memory_order_acquire) + number_of_elements > capacity_) {
        return {};  // Not enough free space in buffer.
    }

    lock write_lock([this, number_of_elements] {
        write_ts_ += number_of_elements;
        size_.fetch_add(number_of_elements, std::memory_order_release);
    });
    write_lock.position.update(write_ts_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::spmc::lock rav::fifo::spmc::prepare_for_read(const size_t number_of_elements) {
    std::unique_lock guard(mutex_);

    if (size_.load(std::memory_order_acquire) < number_of_elements) {
        return {};  // Not enough data available.
    }

    lock read_lock(
        [this, number_of_elements] {
            read_ts_ += number_of_elements;
            size_.fetch_sub(number_of_elements, std::memory_order_release);
        },
        std::move(guard)
    );
    read_lock.position.update(read_ts_, capacity_, number_of_elements);
    return read_lock;
}

void rav::fifo::spmc::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::spmc::reset() {
    read_ts_ = 0;
    write_ts_ = 0;
    size_ = 0;
}

rav::fifo::mpmc::lock rav::fifo::mpmc::prepare_for_write(const size_t number_of_elements) {
    std::unique_lock guard(mutex_);

    if (size_ + number_of_elements > capacity_) {
        return {};  // Not enough free space in buffer.
    }

    lock write_lock(
        [this, number_of_elements] {
            write_ts_ += number_of_elements;
            size_ += number_of_elements;
        },
        std::move(guard)
    );
    write_lock.position.update(write_ts_, capacity_, number_of_elements);
    return write_lock;
}

rav::fifo::mpmc::lock rav::fifo::mpmc::prepare_for_read(const size_t number_of_elements) {
    std::unique_lock guard(mutex_);

    if (size_ < number_of_elements) {
        return {};  // Not enough data available.
    }

    lock read_lock(
        [this, number_of_elements] {
            read_ts_ += number_of_elements;
            size_ -= number_of_elements;
        },
        std::move(guard)
    );
    read_lock.position.update(read_ts_, capacity_, number_of_elements);
    return read_lock;
}

size_t rav::fifo::mpmc::size() const {
    return size_;
}


void rav::fifo::mpmc::resize(const size_t capacity) {
    reset();
    capacity_ = capacity;
}

void rav::fifo::mpmc::reset() {
    read_ts_ = 0;
    write_ts_ = 0;
    size_ = 0;
}
