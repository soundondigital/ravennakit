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
#define RAV_ASSERT_EXCLUSIVE_ACCESS                                    \
    static std::atomic<int32_t> static_counter {0};                    \
    rav::rollback rollback([&] {                                       \
        static_counter.fetch_sub(1, std::memory_order_relaxed);        \
    });                                                                \
    if (static_counter.fetch_add(1, std::memory_order_relaxed) != 0) { \
        RAV_ASSERT_FALSE("Exclusive access violation");                \
    }

#include <atomic>
#include <stdexcept>

namespace rav {

/**
 * Guards exclusive access to a resource. Throws if the resource is already accessed.
 */
class exclusive_access_guard {
  public:
    explicit exclusive_access_guard(std::atomic<int32_t>& counter) : counter_(counter) {
        const auto prev = counter_.fetch_add(1, std::memory_order_relaxed);
        if (prev != 0) {
            counter_.fetch_sub(1, std::memory_order_relaxed);
            throw std::runtime_error("Exclusive access violation");
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
