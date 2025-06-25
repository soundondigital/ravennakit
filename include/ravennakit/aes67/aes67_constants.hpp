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

namespace rav::aes67::constants {

/**
 * The maximum MTU for AES67 packets. From AES67-2023 6.3
 * Note: on connections offering lower MTU than Ethernet’s 1500 bytes, senders can use a smaller maximum payload than
 * specified here.
 */
static constexpr auto k_max_payload = 1440;

}  // namespace rav::aes67::constants
