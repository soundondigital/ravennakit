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
    const auto result = node.start("127.0.0.1", 0);
    if (result.has_error()) {
        RAV_ERROR("Failed to start NMOS node: {}", result.error().message());
        return 1;
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
