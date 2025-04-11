/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include <atomic>
#include <optional>

namespace rav {

/**
 * A single-producer, single-consumer triple buffer. The producer and consumer can be on different threads. Access to
 * the buffer is wait-free for both sides.
 * @tparam T The type of the data to be stored in the buffer.
 */
template<class T>
class TripleBuffer {
  public:
    /**
     * Updates the value.
     * @param value The new value.
     */
    void update(T value) {
        storage_[write_index_] = value;
        write_index_ = next_.exchange(write_index_, std::memory_order_acq_rel);
        write_index_ &= static_cast<uint8_t>(~k_uninit_bit);
    }

    /**
     * @return A new value. If there's nothing to read, returns std::nullopt.
     */
    std::optional<T> get() {
        read_index_ = next_.exchange(read_index_ | k_uninit_bit, std::memory_order_acq_rel);
        if (read_index_ & k_uninit_bit) {
            return std::nullopt;  // If there's nothing to read, return empty optional
        }
        T value = storage_[read_index_];
        return value;
    }

  private:
    static constexpr uint8_t k_uninit_bit = 0b100;
    T storage_[3];
    uint8_t write_index_ {0};
    uint8_t read_index_ {1};
    std::atomic<uint8_t> next_ {2 | k_uninit_bit};
};

}  // namespace rav
