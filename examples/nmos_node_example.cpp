/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/system.hpp"
#include "ravennakit/nmos/nmos_node.hpp"

#include <boost/asio/io_context.hpp>

int main() {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    boost::asio::io_context io_context;

    rav::nmos::Node node(io_context);
    const auto result = node.start("127.0.0.1", 5555);
    if (result.has_error()) {
        RAV_ERROR("Failed to start NMOS node: {}", result.error().message());
        return 1;
    }

    for (uint32_t i = 0; i < 5; ++i) {
        rav::nmos::Device::Control control;
        control.href = fmt::format("http://localhost:{}", i + 6000);
        control.type = fmt::format("urn:x-manufacturer:control:generic.{}", i + 1);
        control.authorization = i % 2 == 0;
        rav::nmos::Device device;
        device.id = boost::uuids::random_generator()();
        device.description = fmt::format("Device {} desc", i + 1);
        device.label = fmt::format("Device {} label", i + 1);
        device.version = rav::nmos::Version {i + 1, (i + 1) * 1000};
        device.controls.push_back(control);
        node.set_device(device);
    }

    std::string url =
        fmt::format("http://{}:{}", node.get_local_endpoint().address().to_string(), node.get_local_endpoint().port());

    RAV_INFO("NMOS node started at {}", url);

    url += "/x-nmos";
    RAV_INFO("{}", url);

    url += "/node";
    RAV_INFO("{}", url);

    url += "/v1.3";
    RAV_INFO("{}", url);

    url += "/";
    RAV_INFO("{}", url);

    io_context.run();

    return 0;
}
