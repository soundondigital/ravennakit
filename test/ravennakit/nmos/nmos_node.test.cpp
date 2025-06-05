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

#include <catch2/catch_all.hpp>

namespace {

class NodeTestRegistryBrowser final: public rav::nmos::RegistryBrowserBase {
  public:
    std::vector<std::tuple<rav::nmos::DiscoverMode, rav::nmos::ApiVersion>> calls_to_start;
    int calls_to_stop = 0;
    mutable int calls_to_find_most_suitable_registry = 0;
    std::optional<rav::dnssd::ServiceDescription> most_suitable_registry;

    void start(rav::nmos::OperationMode operation_mode, rav::nmos::ApiVersion api_version) override {
        calls_to_start.emplace_back(operation_mode, api_version);
    }

    void stop() override {
        calls_to_stop++;
    }

    [[nodiscard]] std::optional<rav::dnssd::ServiceDescription> find_most_suitable_registry() const override {
        calls_to_find_most_suitable_registry++;
        return most_suitable_registry;
    }
};

class NodeTestHttpClient final: public rav::HttpClientBase {
  public:
    void set_host(const boost::urls::url& url) override {
        std::ignore = url;
    }

    void set_host(std::string_view url) override {
        std::ignore = url;
    }

    void set_host(std::string_view host, std::string_view service) override {
        std::ignore = host;
        std::ignore = service;
    }

    void request_async(
        const rav::http::verb method, const std::string_view target, std::string body,
        const std::string_view content_type, const ResponseCallback callback
    ) override {
        std::ignore = method;
        std::ignore = target;
        std::ignore = body;
        std::ignore = content_type;
        rav::http::response<rav::http::string_body> res;
        res.result(rav::http::status::ok);
        callback({res});
    }

    void get_async(std::string_view target, ResponseCallback callback) override {
        std::ignore = target;
        std::ignore = callback;
    }

    void post_async(
        std::string_view target, std::string body, ResponseCallback callback, std::string_view content_type
    ) override {
        std::ignore = target;
        std::ignore = body;
        std::ignore = callback;
        std::ignore = content_type;
    }

    void cancel_outstanding_requests() override {}

    [[nodiscard]] const std::string& get_host() const override {
        static const std::string host = "mock_host";
        return host;
    }

    [[nodiscard]] const std::string& get_service() const override {
        static const std::string service = "mock_service";
        return service;
    }
};

}  // namespace

TEST_CASE("nmos::Node") {
    SECTION("Supported api versions") {
        auto versions = rav::nmos::Node::k_supported_api_versions;
        REQUIRE(versions.size() == 2);
        REQUIRE(versions[0] == rav::nmos::ApiVersion::v1_2());
        REQUIRE(versions[1] == rav::nmos::ApiVersion::v1_3());
    }

    SECTION("Test whether types are printable") {
        std::ignore = fmt::format("{}", rav::nmos::Error::incompatible_discover_mode);
        std::ignore = fmt::format("{}", rav::nmos::OperationMode::registered);
        std::ignore = fmt::format("{}", rav::nmos::DiscoverMode::dns);
    }

    SECTION("Configuration default construction") {
        rav::nmos::Node::Configuration config;
        REQUIRE(config.operation_mode == rav::nmos::OperationMode::registered_p2p);
        REQUIRE(config.discover_mode == rav::nmos::DiscoverMode::dns);
        REQUIRE(config.registry_address.empty());
    }

    SECTION("Test config semantic (validation) rules") {
        rav::nmos::Node::Configuration config;

        REQUIRE(config.validate());

        // Registered and peer-to-peer MAY be used at the same time
        // https://specs.amwa.tv/is-04/releases/v1.3.3/docs/Overview.html#registering-and-discovering-nodes
        config.operation_mode = rav::nmos::OperationMode::registered_p2p;

        SECTION("Validate discover mode in registered_p2p mode") {
            // DNS works for both registered and p2p
            config.discover_mode = rav::nmos::DiscoverMode::dns;
            REQUIRE(config.validate());

            // Multicast DNS works for both registered and p2p
            config.discover_mode = rav::nmos::DiscoverMode::mdns;
            REQUIRE(config.validate());

            // Unicast DNS doesn't work for p2p and is therefore not valid in registered_p2p mode
            config.discover_mode = rav::nmos::DiscoverMode::udns;
            REQUIRE(config.validate() == rav::nmos::Error::incompatible_discover_mode);

            // Manual mode doesn't work for p2p and is therefore not valid in registered_p2p mode
            config.discover_mode = rav::nmos::DiscoverMode::manual;
            REQUIRE(config.validate() == rav::nmos::Error::incompatible_discover_mode);
        }

        config.operation_mode = rav::nmos::OperationMode::registered;

        SECTION("Validate discover mode in registered mode") {
            // DNS works for both registered and p2p
            config.discover_mode = rav::nmos::DiscoverMode::dns;
            REQUIRE(config.validate());

            // Multicast DNS works for both registered and p2p
            config.discover_mode = rav::nmos::DiscoverMode::mdns;
            REQUIRE(config.validate());

            // Unicast DNS works for registered mode
            config.discover_mode = rav::nmos::DiscoverMode::udns;
            REQUIRE(config.validate());

            // Manual mode works for registered mode
            config.discover_mode = rav::nmos::DiscoverMode::manual;

            // Not valid because no address is specified
            REQUIRE(config.validate() == rav::nmos::Error::invalid_registry_address);

            config.registry_address = "http://localhost:8080";

            // Valid because an address is specified
            REQUIRE(config.validate());
        }

        config.operation_mode = rav::nmos::OperationMode::p2p;

        SECTION("Validate discover mode in p2p mode") {
            // DNS doesn't work for p2p and is therefore not valid in p2p mode
            config.discover_mode = rav::nmos::DiscoverMode::dns;
            REQUIRE(config.validate() == rav::nmos::Error::incompatible_discover_mode);

            // Multicast DNS works for both registered and p2p
            config.discover_mode = rav::nmos::DiscoverMode::mdns;
            REQUIRE(config.validate());

            // Unicast DNS doesn't work for p2p and is therefore not valid in p2p mode
            config.discover_mode = rav::nmos::DiscoverMode::udns;
            REQUIRE(config.validate() == rav::nmos::Error::incompatible_discover_mode);

            // Manual mode only works for registered mode and is therefore not valid in p2p mode
            config.discover_mode = rav::nmos::DiscoverMode::manual;
            REQUIRE(config.validate() == rav::nmos::Error::incompatible_discover_mode);

            config.registry_address = "http://localhost:8080";

            // Still not valid because manual mode doesn't work for p2p
            REQUIRE(config.validate() == rav::nmos::Error::incompatible_discover_mode);
        }
    }

    SECTION("Finding and connecting to a registry in registered mode using mdns") {
        boost::asio::io_context io_context;

        rav::nmos::Node::ConfigurationUpdate config_update;
        config_update.operation_mode = rav::nmos::OperationMode::registered;
        config_update.discover_mode = rav::nmos::DiscoverMode::mdns;
        config_update.api_version = rav::nmos::ApiVersion::v1_3();
        config_update.enabled = true;
        config_update.node_api_port = 8080;

        auto test_browser = std::make_unique<NodeTestRegistryBrowser>();
        auto* browser = test_browser.get();

        auto test_http_client = std::make_unique<NodeTestHttpClient>();
        auto* http_client = test_http_client.get();
        REQUIRE(http_client != nullptr);

        rav::nmos::Node node(io_context, std::move(test_browser), std::move(test_http_client));
        auto result = node.update_configuration(config_update, true);
        REQUIRE(result.has_value());
        REQUIRE(browser->calls_to_start.size() == 1);
        REQUIRE(
            browser->calls_to_start[0] == std::make_tuple(rav::nmos::DiscoverMode::mdns, rav::nmos::ApiVersion::v1_3())
        );
        REQUIRE(browser->calls_to_stop == 1);
        REQUIRE(browser->calls_to_find_most_suitable_registry == 0);

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
        browser->most_suitable_registry = desc;
        rav::AsioTimer timer(io_context);
        timer.once(std::chrono::seconds(2), [&node] {
            node.stop();
        });
        io_context.run();
        REQUIRE(browser->calls_to_find_most_suitable_registry == 1);
    }
}
