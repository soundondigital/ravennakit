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
#include "ravennakit/ptp/messages/ptp_announce_message.hpp"
#include "ravennakit/ptp/messages/ptp_message.hpp"
#include "ravennakit/ptp/messages/ptp_message_header.hpp"
#include "ravennakit/rtp/detail/udp_sender_receiver.hpp"

#include <CLI/App.hpp>
#include <asio/io_context.hpp>

int main(int const argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::string interface_address = "0.0.0.0";
    app.add_option("--interface-addr", interface_address, "The interface address");

    CLI11_PARSE(app, argc, argv);

    std::vector<rav::subscription> subscriptions;
    asio::io_context io_context;

    rav::ptp_instance ptp_instance(io_context);
    auto result = ptp_instance.add_port(asio::ip::make_address(interface_address));
    if (!result) {
        RAV_TRACE("PTP Error: {}", static_cast<std::underlying_type_t<rav::ptp_error>>(result.error()));
        return 1;
    }

    io_context.run();

    return 0;
}
