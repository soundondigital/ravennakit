/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/random.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/dnssd/dnssd_config.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

TEST_CASE("dnssd | Browse and advertise") {
    auto random_string = rav::random::generate_random_string(20);
    const auto reg_type = fmt::format("_{}._tcp.", random_string);

    SECTION("On systems other than Apple and Windows, dnssd is not implemented") {
#if !RAV_HAS_DNSSD
        asio::io_context io_context;
        auto advertiser = rav::dnssd::dnssd_advertiser::create(io_context);
        REQUIRE_FALSE(advertiser);
        auto browser = rav::dnssd::dnssd_browser::create(io_context);
        REQUIRE_FALSE(browser);
#endif
    }

    SECTION("Advertise and discover a service") {
#if !RAV_HAS_DNSSD
        return;
#endif
        asio::io_context io_context;

        std::vector<rav::dnssd::service_description> discovered_services;
        std::vector<rav::dnssd::service_description> resolved_services;
        std::vector<rav::dnssd::service_description> removed_services;

        auto advertiser = rav::dnssd::dnssd_advertiser::create(io_context);
        REQUIRE(advertiser);
        const rav::dnssd::txt_record txt_record {{"key1", "value1"}, {"key2", "value2"}};
        auto id = advertiser->register_service(reg_type, "test", nullptr, 1234, txt_record, false);

        auto browser = rav::dnssd::dnssd_browser::create(io_context);
        REQUIRE(browser);
        browser->on<rav::dnssd::dnssd_service_discovered>([&](const rav::dnssd::dnssd_service_discovered& event) {
            discovered_services.push_back(event.description);
        });
        browser->on<rav::dnssd::dnssd_service_resolved>([&](const rav::dnssd::dnssd_service_resolved& event) {
            resolved_services.push_back(event.description);
            advertiser->unregister_service(id);
        });
        browser->on<rav::dnssd::dnssd_service_removed>([&](const rav::dnssd::dnssd_service_removed& event) {
            removed_services.push_back(event.description);
            io_context.stop();
        });

        browser->browse_for(reg_type);

        io_context.run_for(std::chrono::seconds(10));

        REQUIRE(discovered_services.size() == 1);
        REQUIRE(discovered_services.at(0).name == "test");
        REQUIRE(discovered_services.at(0).type == reg_type);
        REQUIRE(discovered_services.at(0).domain == "local.");
        REQUIRE(discovered_services.at(0).port == 0);
        REQUIRE(discovered_services.at(0).txt.empty());
        REQUIRE(discovered_services.at(0).host.empty());

        REQUIRE_FALSE(resolved_services.empty());
        REQUIRE(resolved_services.at(0).name == "test");
        REQUIRE(resolved_services.at(0).type == reg_type);
        REQUIRE(resolved_services.at(0).domain == "local.");
        REQUIRE(resolved_services.at(0).port == 1234);
        REQUIRE(resolved_services.at(0).txt == txt_record);
        REQUIRE_FALSE(resolved_services.at(0).host.empty());

        REQUIRE(removed_services.size() == 1);
        REQUIRE(removed_services.at(0).name == "test");
        REQUIRE(removed_services.at(0).type == reg_type);
        REQUIRE(removed_services.at(0).domain == "local.");
        REQUIRE(removed_services.at(0).port == 1234);
        REQUIRE(removed_services.at(0).txt == txt_record);
        REQUIRE_FALSE(removed_services.at(0).host.empty());
    }

    SECTION("Update a txt record") {}
}

TEST_CASE("dnssd | Update a txt record") {
#if !RAV_HAS_DNSSD
    return;
#endif

    auto random_string = rav::random::generate_random_string(20);
    const auto reg_type = fmt::format("_{}._tcp.", random_string);

    asio::io_context io_context;
    std::optional<rav::dnssd::service_description> updated_service;

    const auto advertiser = rav::dnssd::dnssd_advertiser::create(io_context);
    RAV_ASSERT(advertiser, "Expected a dnssd advertiser");
    const rav::dnssd::txt_record txt_record {{"key1", "value1"}, {"key2", "value2"}};
    const auto id = advertiser->register_service(reg_type, "test", nullptr, 1234, {}, false);

    const auto browser = rav::dnssd::dnssd_browser::create(io_context);
    RAV_ASSERT(browser, "Expected a dnssd browser");
    bool updated = false;
    browser->on<rav::dnssd::dnssd_service_resolved>([&](const rav::dnssd::dnssd_service_resolved& event) {
        if (event.description.txt.empty() && !updated) {
            advertiser->update_txt_record(id, txt_record);
            updated = true;
        }

        if (event.description.txt == txt_record) {
            updated_service = event.description;
            io_context.stop();
        }
    });

    browser->browse_for(reg_type);

    io_context.run_for(std::chrono::seconds(10));

    REQUIRE(updated_service.has_value());
}

TEST_CASE("dnssd | Name collision") {
    // It's tempting to test the name collision feature, but as it turns out the name collision doesn't seem to work on
    // a single host and only between different hosts. To prove this, run this command in two separate terminals:
    // dns-sd -R test _some_service_name._tcp. local. 1234
    // You'll find that both will register the service without any conflict.
}
