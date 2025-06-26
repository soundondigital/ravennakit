/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/interfaces/network_interface_config.hpp"
#include "network_interface_config.test.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::NetworkInterfaceConfig") {
    rav::NetworkInterfaceConfig config;
    config.set_interface(rav::Rank(0), "1");
    config.set_interface(rav::Rank(1), "2");
    test_network_interface_config_json(config, config.to_boost_json());
}

void rav::test_network_interface_config_json(const NetworkInterfaceConfig& config, const boost::json::value& json) {
    REQUIRE(json.is_array());
    REQUIRE(json.as_array().size() == config.interfaces.size());

    for (const auto& i : json.as_array()) {
        REQUIRE(i.is_object());
        auto rank = Rank(i.at("rank").to_number<uint8_t>());
        REQUIRE(i.at("identifier").as_string() == config.interfaces.at(rank));
    }
}
