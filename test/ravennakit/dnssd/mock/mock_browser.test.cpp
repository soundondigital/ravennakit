/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/dnssd/mock/dnssd_mock_browser.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::MockBrowser") {
    boost::asio::io_context io_context;
    rav::dnssd::MockBrowser browser(io_context);

    SECTION("Mock discovering and removing service") {
        std::vector<rav::dnssd::ServiceDescription> discovered_services;
        std::vector<rav::dnssd::ServiceDescription> removed_services;

        browser.on_service_discovered = [&](const auto& desc) {
            discovered_services.push_back(desc);
        };
        browser.on_service_removed = [&](const auto& desc) {
            removed_services.push_back(desc);
        };

        browser.browse_for("reg_type");
        browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
        browser.mock_removed_service("fullname");

        io_context.run();

        REQUIRE(discovered_services.size() == 1);
        REQUIRE(discovered_services[0].fullname == "fullname");
        REQUIRE(discovered_services[0].name == "name");
        REQUIRE(discovered_services[0].reg_type == "reg_type.");
        REQUIRE(discovered_services[0].domain == "domain.");

        REQUIRE(removed_services.size() == 1);
        REQUIRE(discovered_services[0].fullname == "fullname");
        REQUIRE(discovered_services[0].name == "name");
        REQUIRE(discovered_services[0].reg_type == "reg_type.");
        REQUIRE(discovered_services[0].domain == "domain.");
    }

    SECTION("Mock resolving a service") {
        std::vector<rav::dnssd::ServiceDescription> resolved_services;

        browser.on_service_resolved = [&](const auto& desc) {
            resolved_services.push_back(desc);
        };

        browser.browse_for("reg_type");
        browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
        browser.mock_resolved_service("fullname", "host_target", 1234, {{"key", "value"}});

        io_context.run();

        REQUIRE(resolved_services.size() == 1);
        REQUIRE(resolved_services[0].fullname == "fullname");
        REQUIRE(resolved_services[0].name == "name");
        REQUIRE(resolved_services[0].reg_type == "reg_type.");
        REQUIRE(resolved_services[0].domain == "domain.");
        REQUIRE(resolved_services[0].host_target == "host_target");
        REQUIRE(resolved_services[0].port == 1234);
        REQUIRE(resolved_services[0].txt.size() == 1);
        REQUIRE(resolved_services[0].txt.at("key") == "value");
    }

    SECTION("Mock adding removing address") {
        std::vector<rav::dnssd::ServiceDescription> addresses_added;
        std::vector<rav::dnssd::ServiceDescription> addresses_removed;

        browser.on_address_added = [&](const rav::dnssd::ServiceDescription& desc, const std::string& address,
                                       size_t interface_index) {
            std::ignore = address;
            std::ignore = interface_index;
            addresses_added.push_back(desc);
        };
        browser.on_address_removed = [&](const rav::dnssd::ServiceDescription& desc, const std::string& address,
                                         size_t interface_index) {
            std::ignore = address;
            std::ignore = interface_index;
            addresses_removed.push_back(desc);
        };

        browser.browse_for("reg_type");
        browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
        browser.mock_resolved_service("fullname", "host_target", 1234, {});
        browser.mock_added_address("fullname", "address", 1);
        browser.mock_removed_address("fullname", "address", 1);

        io_context.run();

        REQUIRE(addresses_added.size() == 1);
        REQUIRE(addresses_added[0].fullname == "fullname");
        REQUIRE(addresses_added[0].interfaces.size() == 1);
        REQUIRE(addresses_added[0].interfaces.at(1).size() == 1);
        REQUIRE(addresses_added[0].interfaces.at(1).count("address") == 1);

        REQUIRE(addresses_removed.size() == 1);
        REQUIRE(addresses_removed[0].fullname == "fullname");
        REQUIRE(addresses_removed[0].interfaces.empty());
    }

    SECTION("Find service") {
        browser.browse_for("reg_type");
        browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
        REQUIRE(browser.find_service("name") == nullptr);

        boost::asio::post(io_context, [&] {
            auto* service = browser.find_service("name");
            REQUIRE(service != nullptr);
            REQUIRE(service->fullname == "fullname");
            REQUIRE(service->name == "name");
            REQUIRE(service->reg_type == "reg_type.");
            REQUIRE(service->domain == "domain.");
        });

        io_context.run();
    }

    SECTION("Get services") {
        browser.browse_for("reg_type");
        browser.browse_for("reg_type2");
        browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
        browser.mock_discovered_service("fullname2", "name2", "reg_type2", "domain2");
        boost::asio::post(io_context, [&] {
            auto services = browser.get_services();
            REQUIRE(services.size() == 2);
            REQUIRE(services[0].fullname == "fullname");
            REQUIRE(services[0].name == "name");
            REQUIRE(services[0].reg_type == "reg_type.");
            REQUIRE(services[0].domain == "domain.");
            REQUIRE(services[1].fullname == "fullname2");
            REQUIRE(services[1].name == "name2");
            REQUIRE(services[1].reg_type == "reg_type2.");
            REQUIRE(services[1].domain == "domain2.");
        });
        io_context.run();
    }

    SECTION("Subscribe") {
        std::vector<rav::dnssd::ServiceDescription> discovered_services;
        std::vector<rav::dnssd::ServiceDescription> resolved_services;
        std::vector<rav::dnssd::ServiceDescription> addresses_added;

        browser.on_service_discovered = [&](const auto& desc) {
            discovered_services.push_back(desc);
        };
        browser.on_service_resolved = [&](const auto& desc) {
            resolved_services.push_back(desc);
        };
        browser.on_address_added = [&](const rav::dnssd::ServiceDescription& desc, const std::string& address,
                                       size_t interface_index) {
            std::ignore = address;
            std::ignore = interface_index;
            addresses_added.push_back(desc);
        };

        browser.browse_for("reg_type");
        browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
        browser.mock_resolved_service("fullname", "host_target", 1234, {});
        browser.mock_added_address("fullname", "address", 1);

        io_context.run();

        REQUIRE(discovered_services.size() == 1);
        REQUIRE(discovered_services[0].fullname == "fullname");
        REQUIRE(discovered_services[0].name == "name");
        REQUIRE(discovered_services[0].reg_type == "reg_type.");
        REQUIRE(discovered_services[0].domain == "domain.");

        REQUIRE(resolved_services.size() == 1);
        REQUIRE(resolved_services[0].fullname == "fullname");
        REQUIRE(resolved_services[0].host_target == "host_target");
        REQUIRE(resolved_services[0].port == 1234);
        REQUIRE(resolved_services[0].txt.empty());

        REQUIRE(addresses_added.size() == 1);
        REQUIRE(addresses_added[0].fullname == "fullname");
        REQUIRE(addresses_added[0].interfaces.size() == 1);
        REQUIRE(addresses_added[0].interfaces.at(1).size() == 1);
        REQUIRE(addresses_added[0].interfaces.at(1).count("address") == 1);
    }

    SECTION("Mock discovering a service error cases") {
        browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
        REQUIRE_THROWS(io_context.run());
    }

    SECTION("Mock removing address error cases") {
        browser.mock_removed_service("fullname");
        REQUIRE_THROWS(io_context.run());
    }

    SECTION("Mock discovering a service error cases") {
        browser.mock_resolved_service("fullname", "name", 1234, {});
        REQUIRE_THROWS(io_context.run());
    }

    SECTION("Mock adding address error cases") {
        browser.mock_added_address("fullname", "address", 1);
        REQUIRE_THROWS(io_context.run());
    }

    SECTION("Mock removing address error cases") {
        SECTION("Not browsing for") {
            browser.mock_removed_address("fullname", "address", 1);
            REQUIRE_THROWS(io_context.run());
        }

        browser.browse_for("reg_type");

        SECTION("Interface not found") {
            browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
            browser.mock_removed_address("fullname", "address", 1);
            REQUIRE_THROWS(io_context.run());
        }

        SECTION("Address not found") {
            browser.mock_discovered_service("fullname", "name", "reg_type", "domain");
            browser.mock_added_address("fullname", "address", 1);
            browser.mock_removed_address("fullname", "address2", 1);
            REQUIRE_THROWS(io_context.run());
        }
    }

    SECTION("Browse for error cases") {
        browser.browse_for("reg_type");
        REQUIRE_THROWS(browser.browse_for("reg_type"));
    }
}
