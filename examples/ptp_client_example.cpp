/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/system.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"

#include <CLI/App.hpp>
#include <boost/asio/io_context.hpp>

/**
 * This example shows how to create a PTP client.
 */

int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"PTP Client example"};
    argv = app.ensure_utf8(argv);

    std::string interface_address = "0.0.0.0";
    app.add_option("--interface-addr", interface_address, "The interface address");

    CLI11_PARSE(app, argc, argv);

    boost::asio::io_context io_context;

    rav::ptp::Instance ptp_instance(io_context);
    auto result = ptp_instance.add_port(boost::asio::ip::make_address_v4(interface_address));
    if (!result) {
        RAV_TRACE("PTP Error: {}", static_cast<std::underlying_type_t<rav::ptp::Error>>(result.error()));
        return 1;
    }

    while (!io_context.stopped()) {
        io_context.poll();
    }

    return 0;
}
