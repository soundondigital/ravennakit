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
    port_invalid
};

inline const char* to_string(const ptp_error error) {
    switch (error) {
        case ptp_error::invalid_data:
            return "invalid data";
        case ptp_error::invalid_header_length:
            return "invalid header length";
        case ptp_error::invalid_message_length:
            return "invalid message length";
        case ptp_error::only_ordinary_clock_supported:
            return "only ordinary clock supported";
        case ptp_error::only_slave_supported:
            return "only slave supported";
        case ptp_error::failed_to_get_network_interfaces:
            return "failed to get network interfaces";
        case ptp_error::network_interface_not_found:
            return "network interface not found";
        case ptp_error::no_mac_address_available:
            return "no MAC address available";
        case ptp_error::invalid_clock_identity:
            return "invalid clock identity";
        case ptp_error::port_invalid:
            return "port invalid";
        default:
            return "unknown error";
    }
}

}  // namespace rav
