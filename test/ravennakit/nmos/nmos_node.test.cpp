/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/nmos/nmos_node.hpp"
#include "ravennakit/nmos/detail/nmos_test_api_client.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("nmos::Node") {
    SECTION("Supported api versions") {
        auto versions = rav::nmos::Node::k_supported_api_versions;
        REQUIRE(versions.size() == 2);
        REQUIRE(versions[0] == rav::nmos::ApiVersion::v1_2());
        REQUIRE(versions[1] == rav::nmos::ApiVersion::v1_3());
    }

    SECTION("Test whether types are printable") {
        std::ignore = fmt::format("{}", rav::nmos::Node::Error::incompatible_discover_mode);
        std::ignore = fmt::format("{}", rav::nmos::Node::OperationMode::registered);
        std::ignore = fmt::format("{}", rav::nmos::Node::DiscoverMode::dns);
    }

    SECTION("Configuration default construction") {
        rav::nmos::Node::Configuration config;
        REQUIRE(config.operation_mode == rav::nmos::Node::OperationMode::registered_p2p);
        REQUIRE(config.discover_mode == rav::nmos::Node::DiscoverMode::dns);
        REQUIRE(config.registry_address.empty());
    }

    SECTION("Test config semantic (validation) rules") {
        rav::nmos::Node::Configuration config;

        REQUIRE(config.validate());

        // Registered and peer-to-peer MAY be used at the same time
        // https://specs.amwa.tv/is-04/releases/v1.3.3/docs/Overview.html#registering-and-discovering-nodes
        config.operation_mode = rav::nmos::Node::OperationMode::registered_p2p;

        SECTION("Validate discover mode in registered_p2p mode") {
            // DNS works for both registered and p2p
            config.discover_mode = rav::nmos::Node::DiscoverMode::dns;
            REQUIRE(config.validate());

            // Multicast DNS works for both registered and p2p
            config.discover_mode = rav::nmos::Node::DiscoverMode::mdns;
            REQUIRE(config.validate());

            // Unicast DNS doesn't work for p2p and is therefore not valid in registered_p2p mode
            config.discover_mode = rav::nmos::Node::DiscoverMode::udns;
            REQUIRE(config.validate() == rav::nmos::Node::Error::incompatible_discover_mode);

            // Manual mode doesn't work for p2p and is therefore not valid in registered_p2p mode
            config.discover_mode = rav::nmos::Node::DiscoverMode::manual;
            REQUIRE(config.validate() == rav::nmos::Node::Error::incompatible_discover_mode);
        }

        config.operation_mode = rav::nmos::Node::OperationMode::registered;

        SECTION("Validate discover mode in registered mode") {
            // DNS works for both registered and p2p
            config.discover_mode = rav::nmos::Node::DiscoverMode::dns;
            REQUIRE(config.validate());

            // Multicast DNS works for both registered and p2p
            config.discover_mode = rav::nmos::Node::DiscoverMode::mdns;
            REQUIRE(config.validate());

            // Unicast DNS works for registered mode
            config.discover_mode = rav::nmos::Node::DiscoverMode::udns;
            REQUIRE(config.validate());

            // Manual mode works for registered mode
            config.discover_mode = rav::nmos::Node::DiscoverMode::manual;

            // Not valid because no address is specified
            REQUIRE(config.validate() == rav::nmos::Node::Error::invalid_registry_address);

            config.registry_address = "http://localhost:8080";

            // Valid because an address is specified
            REQUIRE(config.validate());
        }

        config.operation_mode = rav::nmos::Node::OperationMode::p2p;

        SECTION("Validate discover mode in p2p mode") {
            // DNS doesn't work for p2p and is therefore not valid in p2p mode
            config.discover_mode = rav::nmos::Node::DiscoverMode::dns;
            REQUIRE(config.validate() == rav::nmos::Node::Error::incompatible_discover_mode);

            // Multicast DNS works for both registered and p2p
            config.discover_mode = rav::nmos::Node::DiscoverMode::mdns;
            REQUIRE(config.validate());

            // Unicast DNS doesn't work for p2p and is therefore not valid in p2p mode
            config.discover_mode = rav::nmos::Node::DiscoverMode::udns;
            REQUIRE(config.validate() == rav::nmos::Node::Error::incompatible_discover_mode);

            // Manual mode only works for registered mode and is therefore not valid in p2p mode
            config.discover_mode = rav::nmos::Node::DiscoverMode::manual;
            REQUIRE(config.validate() == rav::nmos::Node::Error::incompatible_discover_mode);

            config.registry_address = "http://localhost:8080";

            // Still not valid because manual mode doesn't work for p2p
            REQUIRE(config.validate() == rav::nmos::Node::Error::incompatible_discover_mode);
        }
    }
}

namespace {

class NodeRunner {
  public:
    ~NodeRunner() {
        boost::asio::dispatch(io_context_, [this] {
            node_.stop();
        });

        thread_.join();
    }

    boost::asio::ip::tcp::endpoint start() {
        REQUIRE(node_.start("127.0.0.1", 0));
        auto endpoint = node_.get_local_endpoint();

        thread_ = std::thread([this]() {
            io_context_.run();
        });

        return endpoint;
    }

  private:
    boost::asio::io_context io_context_;
    rav::nmos::Node node_ {io_context_};
    std::thread thread_;
};

}  // namespace

TEST_CASE("Test nmos::Node against NMOS Test API") {
    auto url = rav::nmos::NmosTestApiClient::get_test_api_url_from_env();
    if (!url) {
        RAV_WARNING("NMOS_TEST_API_URL environment variable is not set");
        return;
    }

    RAV_INFO("NMOS_TEST_API_URL: {}", *url);

    boost::asio::io_context io_context;
    rav::nmos::NmosTestApiClient client(io_context, *url);
    REQUIRE(client.test_connection());

    NodeRunner runner;
    auto endpoint = runner.start();

    auto result = client.run_test_suite("IS-04-01", endpoint.address().to_string(), endpoint.port(), {"v1.3"});
    REQUIRE(result);
}
