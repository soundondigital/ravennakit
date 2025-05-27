/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/dnssd/mock/dnssd_mock_browser.hpp"
#include "ravennakit/nmos/detail/nmos_registry_browser.hpp"

#include <catch2/catch_all.hpp>

namespace {

class CustomMockBrowser final: public rav::dnssd::MockBrowser {
  public:
    CustomMockBrowser(boost::asio::io_context& io_context, CustomMockBrowser** tracker) :
        MockBrowser(io_context), tracker_(tracker) {
        *tracker_ = this;
    }

    ~CustomMockBrowser() override {
        if (const auto tracker = tracker_) {
            if (*tracker == this) {
                *tracker = nullptr;
            }
        }
    }

  private:
    CustomMockBrowser** tracker_;
};

}  // namespace

TEST_CASE("NMOS Registry Browser") {
    CustomMockBrowser* unicast_browser {};
    CustomMockBrowser* multicast_browser {};

    auto unicast_browser_factory = [&unicast_browser](boost::asio::io_context& io_context) {
        return std::make_unique<CustomMockBrowser>(io_context, &unicast_browser);
    };

    auto multicast_browser_factory = [&multicast_browser](boost::asio::io_context& io_context) {
        return std::make_unique<CustomMockBrowser>(io_context, &multicast_browser);
    };

    boost::asio::io_context io_context;
    rav::nmos::RegistryBrowser browser(
        io_context, std::move(unicast_browser_factory), std::move(multicast_browser_factory)
    );

    SECTION("When using DiscoverMode::dns, both unicast and multicast should be created") {
        browser.start(rav::nmos::DiscoverMode::dns, rav::nmos::ApiVersion::v1_3());
        REQUIRE(unicast_browser);
        REQUIRE(multicast_browser);
        browser.stop();
        REQUIRE_FALSE(unicast_browser);
        REQUIRE_FALSE(multicast_browser);
    }

    SECTION("When using DiscoverMode::udns, only unicast should be created") {
        browser.start(rav::nmos::DiscoverMode::udns, rav::nmos::ApiVersion::v1_3());
        REQUIRE(unicast_browser);
        REQUIRE_FALSE(multicast_browser);
        browser.stop();
        REQUIRE_FALSE(unicast_browser);
        REQUIRE_FALSE(multicast_browser);
    }

    SECTION("When using DiscoverMode::mdns, only multicast should be created") {
        browser.start(rav::nmos::DiscoverMode::mdns, rav::nmos::ApiVersion::v1_3());
        REQUIRE_FALSE(unicast_browser);
        REQUIRE(multicast_browser);
        browser.stop();
        REQUIRE_FALSE(unicast_browser);
        REQUIRE_FALSE(multicast_browser);
    }

    SECTION("When using DiscoverMode::manual, no browser should be created") {
        browser.start(rav::nmos::DiscoverMode::manual, rav::nmos::ApiVersion::v1_3());
        REQUIRE_FALSE(unicast_browser);
        REQUIRE_FALSE(multicast_browser);
        browser.stop();
        REQUIRE_FALSE(unicast_browser);
        REQUIRE_FALSE(multicast_browser);
    }

    SECTION("Discover mdns service") {
        browser.start(rav::nmos::DiscoverMode::mdns, rav::nmos::ApiVersion::v1_3());

        int times_called = 0;
        browser.on_registry_discovered = [&](const rav::dnssd::ServiceDescription& desc) {
            times_called++;
            REQUIRE(desc.reg_type == "_nmos-register._tcp.");
            REQUIRE(desc.fullname == "multicast_service");
            REQUIRE(desc.domain == "local.");
            REQUIRE(desc.txt.find("api_proto")->second == "http");
            REQUIRE(desc.txt.find("api_ver")->second == "v1.3");
            REQUIRE(desc.txt.find("api_auth")->second == "false");
            REQUIRE(desc.txt.find("pri")->second == "100");
        };

        multicast_browser->mock_discovered_service(
            "multicast_service", "multicast_service_name", "_nmos-register._tcp", "local"
        );
        multicast_browser->mock_resolved_service(
            "multicast_service", "_nmos-register.local.", 1234,
            {{"api_proto", "http"}, {"api_ver", "v1.3"}, {"api_auth", "false"}, {"pri", "100"}}
        );

        io_context.run();

        REQUIRE(times_called == 1);
    }

    SECTION("Discover udns service") {
        browser.start(rav::nmos::DiscoverMode::udns, rav::nmos::ApiVersion::v1_3());

        unicast_browser->mock_discovered_service(
            "unicast_service", "unicast_service_name", "_nmos-register._tcp", "some.domain"
        );
        unicast_browser->mock_resolved_service(
            "unicast_service", "_nmos-register.local.", 1234,
            {{"api_proto", "http"}, {"api_ver", "v1.3"}, {"api_auth", "false"}, {"pri", "100"}}
        );

        io_context.run();

        auto desc = browser.find_most_suitable_registry();
        REQUIRE(desc.has_value());
        REQUIRE(desc->reg_type == "_nmos-register._tcp.");
        REQUIRE(desc->fullname == "unicast_service");
        REQUIRE(desc->domain == "some.domain.");
        REQUIRE(desc->txt.find("api_proto")->second == "http");
        REQUIRE(desc->txt.find("api_ver")->second == "v1.3");
        REQUIRE(desc->txt.find("api_auth")->second == "false");
        REQUIRE(desc->txt.find("pri")->second == "100");
    }

    SECTION("find_most_suitable_registry") {
        browser.start(rav::nmos::DiscoverMode::dns, rav::nmos::ApiVersion::v1_3());
        multicast_browser->mock_discovered_service("service1", "service1_name", "_nmos-register._tcp", "local");
        multicast_browser->mock_resolved_service(
            "service1", "_nmos-register.local.", 1234,
            {{"api_proto", "http"}, {"api_ver", "v1.3"}, {"api_auth", "false"}, {"pri", "100"}}
        );
        unicast_browser->mock_discovered_service("service2", "service2_name", "_nmos-register._tcp", "some.domain");
        unicast_browser->mock_resolved_service(
            "service2", "_nmos-register.local.", 1234,
            {{"api_proto", "http"}, {"api_ver", "v1.3"}, {"api_auth", "false"}, {"pri", "100"}}
        );

        io_context.run();

        // Unicast service should be preferred over multicast

        auto desc = browser.find_most_suitable_registry();
        REQUIRE(desc.has_value());
        REQUIRE(desc->reg_type == "_nmos-register._tcp.");
        REQUIRE(desc->fullname == "service2");
        REQUIRE(desc->domain == "some.domain.");
        REQUIRE(desc->txt.find("api_proto")->second == "http");
        REQUIRE(desc->txt.find("api_ver")->second == "v1.3");
        REQUIRE(desc->txt.find("api_auth")->second == "false");
        REQUIRE(desc->txt.find("pri")->second == "100");
    }

    SECTION("Don't discover invalid services") {
        browser.start(rav::nmos::DiscoverMode::dns, rav::nmos::ApiVersion::v1_3());
        browser.on_registry_discovered = [](const rav::dnssd::ServiceDescription&) {
            FAIL("Should not discover invalid service");
        };
        multicast_browser->mock_discovered_service(
            "invalid_service", "invalid_service_name", "_nmos-register._tcp", "local"
        );
        SECTION("Invalid proto") {
            multicast_browser->mock_resolved_service(
                "invalid_service", "_nmos-register.local.", 1234,
                {{"api_proto", "https"}, {"api_ver", "v1.3"}, {"api_auth", "false"}, {"pri", "100"}}
            );
        }

        SECTION("Invalid api_ver") {
            multicast_browser->mock_resolved_service(
                "invalid_service", "_nmos-register.local.", 1234,
                {{"api_proto", "http"}, {"api_ver", "v1.2"}, {"api_auth", "false"}, {"pri", "100"}}
            );
        }

        SECTION("Invalid api_auth") {
            multicast_browser->mock_resolved_service(
                "invalid_service", "_nmos-register.local.", 1234,
                {{"api_proto", "http"}, {"api_ver", "v1.3"}, {"api_auth", "false!"}, {"pri", "100"}}
            );
        }

        SECTION("Invalid pri") {
            multicast_browser->mock_resolved_service(
                "invalid_service", "_nmos-register.local.", 1234,
                {{"api_proto", "http"}, {"api_ver", "v1.3"}, {"api_auth", "false"}, {"pri", "n/a"}}
            );
        }
    }
}
