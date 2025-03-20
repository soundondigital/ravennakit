/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/core/net/interfaces/network_interface.hpp"

/**
 * This example shows the available network interfaces on the system.
 */

int main() {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    auto ifaces = rav::network_interface::get_all();
    for (auto& iface : ifaces.value()) {
        fmt::println("{}", iface.to_string());
    }

    return 0;
}
