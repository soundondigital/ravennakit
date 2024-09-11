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

/**
 * Encapsulates the regions of a FIFO buffer that are being read or written to.
 */
struct position {
    size_t index1 {};
    size_t size1 {};
    size_t size2 {};

    /**
     * Updates the position with the given parameters.
     * @param pointer The head or tail pointer.
     * @param capacity The total capacity of the buffer.
     * @param number_of_elements The number of elements to read or write.
     */
    void update(size_t pointer, size_t capacity, size_t number_of_elements);
};

/**
 * A fifo without any synchronization. Can be used in single-threaded environments.
 */
struct single {
    /**
     * A lock returned by a prepared oparation.
     */
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(single* owner) : owner_(owner) {}

        /**
         * Returns true if this lock is valid, or false if not.
         */
        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        single* owner_ {nullptr};
    };

    /**
     * Prepares for writing.
     *
     * Attempts to acquire a lock for writing `number_of_elements` to the FIFO buffer. If sufficient space is available,
     * a valid lock is returned, reserving the required buffer space for the duration of the lock. If space is
     * insufficient, an invalid lock is returned.
     *
     * Thread safe: no
     * Realtime safe: yes
     *
     * @param number_of_elements The number of elements to write.
     * @return A valid lock if space is available; otherwise, an invalid lock.
     */
    lock prepare_for_write(size_t number_of_elements);

    /**
     * Prepares for reading.
     *
     * Attempts to acquire a lock for reading `number_of_elements` from the FIFO buffer. If sufficient data is
     * available, a valid lock is returned, reserving the required buffer space for the duration of the lock. If there
     * is not enough data available, an invalid lock is returned.
     *
     * Thread safe: no
     * Realtime safe: yes
     *
     * @param number_of_elements The number of elements to read.
     * @return A valid lock if sufficient data is available; otherwise, an invalid lock.
     */
    lock prepare_for_read(size_t number_of_elements);

    /**
     * Thread safe: no
     * Realtime safe: yes
     * @return The number of elements in the buffer.
     */
    [[nodiscard]] size_t size() const;

    /**
     * Commits a write operation.
     * Thread safe: no
     * Realtime safe: yes
     * @param lock The lock with the write position to commit.
     */
    void commit_write(const lock& lock);

    /**
     * Commits a read operation.
     * Thread safe: no
     * Realtime safe: yes
     * @param lock The lock with the read position to commit.
     */
    void commit_read(const lock& lock);

    /**
     * Resizes the buffer. Implies a reset.
     * Thread safe: no
     * Realtime safe: no
     * @param capacity The new capacity of the buffer.
     */
    void resize(size_t capacity);

    /**
     * Resets the buffer, discarding existing contents.
     * Thread safe: no
     * Realtime safe: yes
     */
    void reset();

  private:
    size_t head_ = 0;  // Consumer index
    size_t tail_ = 0;  // Producer index
    size_t capacity_ = 0;
    size_t size_ = 0;
};

/**
 * A fifo which a single producer and single consumer thread can simultaneously read and write to.
 */
struct spsc {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(spsc* owner) : owner_(owner) {}

        /**
         * Returns true if this lock is valid, or false if not.
         */
        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        spsc* owner_ {nullptr};
    };

    /**
     * Prepares for writing.
     *
     * Attempts to acquire a lock for writing `number_of_elements` to the FIFO buffer. If sufficient space is available,
     * a valid lock is returned, reserving the required buffer space for the duration of the lock. If space is
     * insufficient, an invalid lock is returned.
     *
     * Thread safe: yes when used from a single producer thread
     * Realtime safe: yes
     *
     * @param number_of_elements The number of elements to write.
     * @return A valid lock if space is available; otherwise, an invalid lock.
     */
    lock prepare_for_write(size_t number_of_elements);

    /**
     * Prepares for reading.
     *
     * Attempts to acquire a lock for reading `number_of_elements` from the FIFO buffer. If sufficient data is
     * available, a valid lock is returned, reserving the required buffer space for the duration of the lock. If there
     * is not enough data available, an invalid lock is returned.
     *
     * Thread safe: yes when used from a single consumer thread
     * Realtime safe: yes
     *
     * @param number_of_elements The number of elements to read.
     * @return A valid lock if sufficient data is available; otherwise, an invalid lock.
     */
    lock prepare_for_read(size_t number_of_elements);

    /**
     * Commits a write operation.
     * @param lock The lock with the write position to commit.
     */
    void commit_write(const lock& lock);

    /**
     * Commits a read operation.
     * @param lock The lock with the read position to commit.
     */
    void commit_read(const lock& lock);

    /**
     * Resizes the buffer. Implies a reset.
     * Thread safe: no
     * Realtime safe: no
     * @param capacity The new capacity of the buffer.
     */
    void resize(size_t capacity);

    /**
     * Resets the buffer, discarding existing contents.
     */
    void reset();

  private:
    size_t head_ = 0;               // Consumer index
    size_t tail_ = 0;               // Producer index
    std::atomic<size_t> size_ = 0;  // Number of elements in the buffer
    size_t capacity_ = 0;
};

/**
 * A fifo where multiple producer threads can write to the buffer, but only a single consumer thread can read from it.
 */
struct mpsc {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(mpsc* owner) : owner_(owner) {}

        explicit lock(mpsc* owner, std::unique_lock<std::mutex>&& unique_lock) :
            owner_(owner), lock_(std::move(unique_lock)) {}

        /**
         * Returns true if this lock is valid, or false if not.
         */
        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        mpsc* owner_ {nullptr};
        std::unique_lock<std::mutex> lock_;
    };

    /**
     * Prepares for writing.
     *
     * Attempts to acquire a lock for writing `number_of_elements` to the FIFO buffer. If sufficient space is available,
     * a valid lock is returned, reserving the required buffer space for the duration of the lock. If space is
     * insufficient, an invalid lock is returned.
     *
     * Thread safe: yes
     * Realtime safe: no
     *
     * @param number_of_elements The number of elements to write.
     * @return A valid lock if space is available; otherwise, an invalid lock.
     */
    lock prepare_for_write(size_t number_of_elements);

    /**
     * Prepares for reading.
     *
     * Attempts to acquire a lock for reading `number_of_elements` from the FIFO buffer. If sufficient data is
     * available, a valid lock is returned, reserving the required buffer space for the duration of the lock. If there
     * is not enough data available, an invalid lock is returned.
     *
     * Thread safe: yes when used from a single consumer thread
     * Realtime safe: yes
     *
     * @param number_of_elements The number of elements to read.
     * @return A valid lock if sufficient data is available; otherwise, an invalid lock.
     */
    lock prepare_for_read(size_t number_of_elements);

    /**
     * Commits a write operation.
     * @param lock The lock with the write position to commit.
     */
    void commit_write(const lock& lock);

    /**
     * Commits a read operation.
     * @param lock The lock with the read position to commit.
     */
    void commit_read(const lock& lock);

    /**
     * Resizes the buffer. Implies a reset.
     * Thread safe: no
     * Realtime safe: no
     * @param capacity The new capacity of the buffer.
     */
    void resize(size_t capacity);

    /**
     * Resets the buffer, discarding existing contents.
     */
    void reset();

  private:
    size_t head_ = 0;  // Consumer index
    size_t tail_ = 0;  // Producer index
    size_t capacity_ = 0;
    std::atomic<size_t> size_ = 0;  // Number of elements in the buffer
    std::mutex mutex_;
};

/**
 * A fifo where a single producer thread and multiple consumer threads can simultaneously read and write to the buffer.
 */
struct spmc {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(spmc* owner) : owner_(owner) {}

        explicit lock(spmc* owner, std::unique_lock<std::mutex>&& unique_lock) :
            owner_(owner), lock_(std::move(unique_lock)) {}

        /**
         * Returns true if this lock is valid, or false if not.
         */
        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        spmc* owner_ {nullptr};
        std::unique_lock<std::mutex> lock_;
    };

    /**
     * Prepares for writing.
     *
     * Attempts to acquire a lock for writing `number_of_elements` to the FIFO buffer. If sufficient space is available,
     * a valid lock is returned, reserving the required buffer space for the duration of the lock. If space is
     * insufficient, an invalid lock is returned.
     *
     * Thread safe: yes when used from a single producer thread
     * Realtime safe: yes
     *
     * @param number_of_elements The number of elements to write.
     * @return A valid lock if space is available; otherwise, an invalid lock.
     */
    lock prepare_for_write(size_t number_of_elements);

    /**
     * Prepares for reading.
     *
     * Attempts to acquire a lock for reading `number_of_elements` from the FIFO buffer. If sufficient data is
     * available, a valid lock is returned, reserving the required buffer space for the duration of the lock. If there
     * is not enough data available, an invalid lock is returned.
     *
     * Thread safe: yes
     * Realtime safe: no
     *
     * @param number_of_elements The number of elements to read.
     * @return A valid lock if sufficient data is available; otherwise, an invalid lock.
     */
    lock prepare_for_read(size_t number_of_elements);

    /**
     * Commits a write operation.
     * @param lock The lock with the write position to commit.
     */
    void commit_write(const lock& lock);

    /**
     * Commits a read operation.
     * @param lock The lock with the read position to commit.
     */
    void commit_read(const lock& lock);

    /**
     * Resizes the buffer. Implies a reset.
     * Thread safe: no
     * Realtime safe: no
     * @param capacity The new capacity of the buffer.
     */
    void resize(size_t capacity);

    /**
     * Resets the buffer, discarding existing contents.
     */
    void reset();

  private:
    size_t head_ = 0;  // Consumer index
    size_t tail_ = 0;  // Producer index
    size_t capacity_ = 0;
    std::atomic<size_t> size_ = 0;  // Number of elements in the buffer
    std::mutex mutex_;
};

/**
 * A fifo where multiple producer and multiple consumer threads can simultaneously read and write to the buffer.
 */
struct mpmc {
    struct lock {
        fifo::position position {};

        lock() = default;

        explicit lock(mpmc* owner, std::unique_lock<std::mutex>&& unique_lock) :
            owner_(owner), lock_(std::move(unique_lock)) {}

        /**
         * Returns true if this lock is valid, or false if not.
         */
        explicit operator bool() const {
            return owner_ != nullptr;
        }

      private:
        mpmc* owner_ {nullptr};
        std::unique_lock<std::mutex> lock_;
    };

    /**
     * Prepares for writing.
     *
     * Attempts to acquire a lock for writing `number_of_elements` to the FIFO buffer. If sufficient space is available,
     * a valid lock is returned, reserving the required buffer space for the duration of the lock. If space is
     * insufficient, an invalid lock is returned.
     *
     * Thread safe: yes
     * Realtime safe: no
     *
     * @param number_of_elements The number of elements to write.
     * @return A valid lock if space is available; otherwise, an invalid lock.
     */
    lock prepare_for_write(size_t number_of_elements);

    /**
     * Prepares for reading.
     *
     * Attempts to acquire a lock for reading `number_of_elements` from the FIFO buffer. If sufficient data is
     * available, a valid lock is returned, reserving the required buffer space for the duration of the lock. If there
     * is not enough data available, an invalid lock is returned.
     *
     * Thread safe: yes
     * Realtime safe: no
     *
     * @param number_of_elements The number of elements to read.
     * @return A valid lock if sufficient data is available; otherwise, an invalid lock.
     */
    lock prepare_for_read(size_t number_of_elements);

    /**
     * Commits a write operation.
     * @param lock The lock with the write position to commit.
     */
    void commit_write(const lock& lock);

    /**
     * Commits a read operation.
     * @param lock The lock with the read position to commit.
     */
    void commit_read(const lock& lock);

    /**
     * Resizes the buffer. Implies a reset.
     * Thread safe: no
     * Realtime safe: no
     * @param capacity The new capacity of the buffer.
     */
    void resize(size_t capacity);

    /**
     * Resets the buffer, discarding existing contents.
     */
    void reset();

  private:
    size_t head_ = 0;  // Consumer index
    size_t tail_ = 0;  // Producer index
    size_t capacity_ = 0;
    size_t size_ = 0;  // Number of elements in the buffer
    std::mutex mutex_;
};

}  // namespace rav::fifo
