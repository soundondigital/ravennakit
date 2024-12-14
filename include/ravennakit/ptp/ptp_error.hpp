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

namespace rav {

enum class ptp_error {
    invalid_data,
    invalid_header_length,
    invalid_message_length,
    only_ordinary_clock_supported,
    only_slave_supported,
    failed_to_get_network_interfaces,
    network_interface_not_found,
    no_mac_address_available,
    invalid_clock_identity,
};

}
