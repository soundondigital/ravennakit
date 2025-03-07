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
#define RAV_ASSERT_EXCLUSIVE_ACCESS(guard)                           \
    rav::exclusive_access_guard::lock CONCAT(lock, __LINE__)(guard); \
    RAV_ASSERT(!CONCAT(lock, __LINE__).violated(), "exclusive access violation");

#include <atomic>
#include <stdexcept>

namespace rav {

/**
 * Guards exclusive access to a resource. Throws if the resource is already accessed.
 */
class exclusive_access_guard {
  public:
    /**
     * Locks and exclusive access guard.
     */
    class lock {
      public:
        /**
         * Constructs a new lock.
         * @throws std::runtime_error if exclusive access is violated.
         */
        explicit lock(exclusive_access_guard& access_guard) : exclusive_access_guard_(access_guard) {
            const auto prev = exclusive_access_guard_.counter_.fetch_add(1, std::memory_order_relaxed);
            if (prev != 0) {
                violated_ = true;
            }
        }

        ~lock() {
            exclusive_access_guard_.counter_.fetch_sub(1, std::memory_order_relaxed);
        }

        /**
         * @return True if exclusive access is violated.
         */
        [[nodiscard]] bool violated() const {
            return violated_;
        }

      private:
        exclusive_access_guard& exclusive_access_guard_;
        bool violated_ = false;
    };

    /**
     * Constructs a new exclusive access guard.
     */
    explicit exclusive_access_guard() = default;

    exclusive_access_guard(const exclusive_access_guard&) = delete;
    exclusive_access_guard& operator=(const exclusive_access_guard&) = delete;

    exclusive_access_guard(exclusive_access_guard&&) = delete;
    exclusive_access_guard& operator=(exclusive_access_guard&&) = delete;

  private:
    std::atomic<int8_t> counter_ {};
};

}  // namespace rav
