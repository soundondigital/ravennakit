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
#include "ravennakit/core/rollback.hpp"

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b

/**
 * Helper macro which asserts exclusive access to scope. Whenever 2 different threads access the scope, an assertion
 * will be triggered.
 */
#define RAV_ASSERT_EXCLUSIVE_ACCESS(counter)                                           \
    rav::exclusive_access_guard CONCAT(exclusive_access_guard, __LINE__)(counter, [] { \
        RAV_ASSERT(false, "Exclusive access violation");                               \
    });

#include <atomic>
#include <stdexcept>

namespace rav {

/**
 * Guards exclusive access to a resource. Throws if the resource is already accessed.
 */
class exclusive_access_guard {
  public:
    /**
     * Constructs a new exclusive access guard.
     * @throws rav::exception if exclusive access is violated.
     * @param counter The counter to guard.
     */
    explicit exclusive_access_guard(std::atomic<int32_t>& counter) : counter_(counter) {
        const auto prev = counter_.fetch_add(1, std::memory_order_relaxed);
        if (prev != 0) {
            counter_.fetch_sub(1, std::memory_order_relaxed);
            throw std::runtime_error("Exclusive access violation");
        }
    }

    /**
     * Constructs a new exclusive access guard.
     * @throws rav::exception if exclusive access is violated and no on_violation callback is provided.
     * @param counter The counter to guard.
     * @param on_violation Callback to be called when exclusive access is violated.
     */
    explicit exclusive_access_guard(std::atomic<int32_t>& counter, const std::function<void()>& on_violation) :
        counter_(counter) {
        const auto prev = counter_.fetch_add(1, std::memory_order_relaxed);
        if (prev != 0) {
            if (on_violation) {
                on_violation();
            } else {
                counter_.fetch_sub(1, std::memory_order_relaxed);
                throw std::runtime_error("Exclusive access violation");
            }
        }
    }

    ~exclusive_access_guard() {
        counter_.fetch_sub(1, std::memory_order_relaxed);
    }

    exclusive_access_guard(const exclusive_access_guard&) = delete;
    exclusive_access_guard& operator=(const exclusive_access_guard&) = delete;

    exclusive_access_guard(exclusive_access_guard&&) = delete;
    exclusive_access_guard& operator=(exclusive_access_guard&&) = delete;

  private:
    std::atomic<int32_t>& counter_;
};

}  // namespace rav
