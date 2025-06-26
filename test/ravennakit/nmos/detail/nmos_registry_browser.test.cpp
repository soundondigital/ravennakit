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

TEST_CASE("rav::nmos::RegistryBrowser") {
    CustomMockBrowser* multicast_browser {};

    auto multicast_browser_factory = [&multicast_browser](boost::asio::io_context& io_context) {
        return std::make_unique<CustomMockBrowser>(io_context, &multicast_browser);
    };

    boost::asio::io_context io_context;
    rav::nmos::RegistryBrowser browser(io_context, std::move(multicast_browser_factory));

    SECTION("When using OperationMode::mdns_p2p, a multicast browser should be created") {
        browser.start(rav::nmos::OperationMode::mdns_p2p, rav::nmos::ApiVersion {1, 3});
        REQUIRE(multicast_browser);
        browser.stop();
        REQUIRE_FALSE(multicast_browser);
    }

    SECTION("When using OperationMode::manual, no browser should be created") {
        browser.start(rav::nmos::OperationMode::manual, rav::nmos::ApiVersion {1, 3});
        REQUIRE_FALSE(multicast_browser);
        browser.stop();
        REQUIRE_FALSE(multicast_browser);
    }

    SECTION("When using OperationMode::p2p, no browser should be created") {
        browser.start(rav::nmos::OperationMode::p2p, rav::nmos::ApiVersion {1, 3});
        REQUIRE_FALSE(multicast_browser);
        browser.stop();
        REQUIRE_FALSE(multicast_browser);
    }

    SECTION("Discover mdns service") {
        browser.start(rav::nmos::OperationMode::mdns_p2p, rav::nmos::ApiVersion {1, 3});

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

    SECTION("find_most_suitable_registry") {
        browser.start(rav::nmos::OperationMode::mdns_p2p, rav::nmos::ApiVersion {1, 3});
        multicast_browser->mock_discovered_service("service1", "service1_name", "_nmos-register._tcp", "local.");
        multicast_browser->mock_resolved_service(
            "service1", "_nmos-register.local.", 1234,
            {{"api_proto", "http"}, {"api_ver", "v1.3"}, {"api_auth", "false"}, {"pri", "100"}}
        );

        io_context.run();

        auto desc = browser.find_most_suitable_registry();
        REQUIRE(desc.has_value());
        REQUIRE(desc->reg_type == "_nmos-register._tcp.");
        REQUIRE(desc->fullname == "service1");
        REQUIRE(desc->domain == "local.");
        REQUIRE(desc->txt.find("api_proto")->second == "http");
        REQUIRE(desc->txt.find("api_ver")->second == "v1.3");
        REQUIRE(desc->txt.find("api_auth")->second == "false");
        REQUIRE(desc->txt.find("pri")->second == "100");
    }

    SECTION("Don't discover invalid services") {
        browser.start(rav::nmos::OperationMode::mdns_p2p, rav::nmos::ApiVersion {1, 3});
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

    SECTION("filter_and_get_pri") {
        rav::dnssd::ServiceDescription desc;
        desc.fullname = "registry._nmos-register._tcp.local.";
        desc.name = "registry";
        desc.reg_type = "_nmos-register._tcp.";
        desc.domain = "local.";
        desc.host_target = "machine.local.";
        desc.port = 8080;
        desc.txt = {
            {"api_proto", "http"},
            {"api_ver", "v1.2,v1.3"},
            {"api_auth", "false"},
            {"pri", "100"},
        };

        SECTION("Valid service") {
            auto pri = rav::nmos::RegistryBrowserBase::filter_and_get_pri(desc, rav::nmos::ApiVersion {1, 3});
            REQUIRE(pri.has_value());
            REQUIRE(*pri == 100);
        }

        SECTION("Valid service") {
            desc.reg_type = "_nmos-registration._tcp.";
            auto pri = rav::nmos::RegistryBrowserBase::filter_and_get_pri(desc, rav::nmos::ApiVersion {1, 3});
            REQUIRE(pri.has_value());
            REQUIRE(*pri == 100);
        }

        SECTION("Invalid reg_type") {
            desc.reg_type = "_nmos-invalid._tcp.";
            REQUIRE_FALSE(rav::nmos::RegistryBrowserBase::filter_and_get_pri(desc, rav::nmos::ApiVersion {1, 3}));
        }

        SECTION("Invalid api_proto") {
            desc.txt["api_proto"] = "https";  // Not supported
            REQUIRE_FALSE(rav::nmos::RegistryBrowserBase::filter_and_get_pri(desc, rav::nmos::ApiVersion {1, 3}));
        }

        SECTION("Invalid api_ver") {
            desc.txt["api_ver"] = "v1.0,v1.1,v1.2";
            REQUIRE_FALSE(rav::nmos::RegistryBrowserBase::filter_and_get_pri(desc, rav::nmos::ApiVersion {1, 3}));
        }

        SECTION("Invalid api_auth") {
            desc.txt["api_auth"] = "true";  // Not supported
            REQUIRE_FALSE(rav::nmos::RegistryBrowserBase::filter_and_get_pri(desc, rav::nmos::ApiVersion {1, 3}));
        }

        SECTION("Invalid pri") {
            desc.txt["pri"] = "n/a";  // Not a number
            REQUIRE_FALSE(rav::nmos::RegistryBrowserBase::filter_and_get_pri(desc, rav::nmos::ApiVersion {1, 3}));
        }
    }
}
