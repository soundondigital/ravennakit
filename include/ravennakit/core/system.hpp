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

#include "ravennakit/core/byte_order.hpp"

#include <cstdint>

namespace rav::system {

inline void do_system_checks() {
    static_assert(sizeof(size_t) == sizeof(uint64_t), "size_t must be 64-bit");
    byte_order::validate_byte_order();
}

}  // namespace rav::system
