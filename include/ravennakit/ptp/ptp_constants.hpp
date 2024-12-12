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

#include <cstdint>

namespace rav {

constexpr uint8_t k_foreign_master_time_window = 4;  // times announce interval
constexpr uint8_t k_foreign_master_threshold = 2;    // Announce messages received within time window

}  // namespace rav
