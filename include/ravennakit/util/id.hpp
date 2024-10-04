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

#include <cstdint>

namespace rav::util {

class id {
  public:
    class generator {
      public:
        [[nodiscard]] id next() {
            RAV_ASSERT(next_id_ != 0, "Next ID is 0, which is reserved for invalid IDs");
            RAV_ASSERT(next_id_ != std::numeric_limits<uint64_t>::max(), "The next ID is at the maximum value");
            return id(next_id_++);
        }

      private:
        uint64_t next_id_ {1};
    };

    id() = default;
    explicit id(const uint64_t int_id) : id_(int_id) {}

    id(const id& other) = default;
    id(id&& other) noexcept = default;
    id& operator=(const id& other) = default;
    id& operator=(id&& other) noexcept = default;

    [[nodiscard]] bool is_valid() const noexcept {
        return id_ != 0;
    }

    bool operator==(const uint64_t value) const {
        return id_ == value;
    }

    friend bool operator==(const id& lhs, const id& rhs) {
        return lhs.id_ == rhs.id_;
    }

    friend bool operator!=(const id& lhs, const id& rhs) {
        return !(lhs == rhs);
    }

    static id next_process_wide_unique_id() noexcept {
        static generator gen;
        return gen.next();
    }

  private:
    uint64_t id_ {};
};

}  // namespace rav::util
