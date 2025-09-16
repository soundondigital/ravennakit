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
#include "nmos_node.test.hpp"

#include <catch2/catch_all.hpp>

namespace {

class NodeTestRegistryBrowser final: public rav::nmos::RegistryBrowserBase {
  public:
    std::vector<std::tuple<rav::nmos::OperationMode, rav::nmos::ApiVersion>> calls_to_start;
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

    void delete_async(std::string_view target, ResponseCallback callback) override {
        std::ignore = target;
        std::ignore = callback;
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

TEST_CASE("rav::nmos::Node") {
    SECTION("Supported api versions") {
        auto versions = rav::nmos::Node::k_node_api_versions;
        REQUIRE(versions.size() == 2);
        REQUIRE(versions[0] == rav::nmos::ApiVersion {1, 2});
        REQUIRE(versions[1] == rav::nmos::ApiVersion {1, 3});
    }

    SECTION("Test whether types are printable") {
        std::ignore = fmt::format("{}", rav::nmos::Error::failed_to_start_http_server);
        std::ignore = fmt::format("{}", rav::nmos::OperationMode::mdns_p2p);
    }

    SECTION("Configuration default construction") {
        rav::nmos::Node::Configuration config;
        REQUIRE(config.operation_mode == rav::nmos::OperationMode::mdns_p2p);
        REQUIRE(config.registry_address.empty());
    }

    SECTION("Test config semantic (validation) rules") {
        rav::nmos::Node::Configuration config;
        REQUIRE(config.validate() == rav::nmos::Error::invalid_id);

        config.id = boost::uuids::random_generator()();

        REQUIRE(config.validate());

        config.operation_mode = rav::nmos::OperationMode::manual;

        SECTION("Validate discover mode in registered mode") {
            // Not valid because no address is specified
            REQUIRE(config.validate() == rav::nmos::Error::no_registry_address_given);
            config.registry_address = "not-a-valid-address";
            REQUIRE(config.validate() == rav::nmos::Error::invalid_registry_address);
            config.registry_address = "http://localhost:8080";
            REQUIRE(config.validate());
        }
    }

    SECTION("Finding and connecting to a registry in registered mode using mdns") {
        boost::asio::io_context io_context;

        rav::nmos::Node::Configuration config;
        config.id = boost::uuids::random_generator()();
        config.operation_mode = rav::nmos::OperationMode::mdns_p2p;
        config.api_version = rav::nmos::ApiVersion {1, 3};
        config.enabled = true;
        config.api_port = 8080;

        auto test_browser = std::make_unique<NodeTestRegistryBrowser>();
        auto* browser = test_browser.get();

        auto test_http_client = std::make_unique<NodeTestHttpClient>();
        auto* http_client = test_http_client.get();
        REQUIRE(http_client != nullptr);

        rav::ptp::Instance ptp_instance(io_context);

        rav::nmos::Node node(io_context, ptp_instance, std::move(test_browser), std::move(test_http_client));
        auto result = node.set_configuration(config, true);
        REQUIRE(result);
        REQUIRE(browser->calls_to_start.size() == 1);
        REQUIRE(
            browser->calls_to_start[0]
            == std::make_tuple(rav::nmos::OperationMode::mdns_p2p, rav::nmos::ApiVersion {1, 3})
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

    SECTION("Add and remove a device") {
        boost::asio::io_context io_context;
        rav::ptp::Instance ptp_instance(io_context);
        rav::nmos::Node node(io_context, ptp_instance);

        rav::nmos::Device device;
        device.id = boost::uuids::random_generator()();
        device.label = "Test Device";
        device.description = "A test device";

        REQUIRE(node.add_or_update_device(&device));

        auto devices = node.get_devices();
        REQUIRE(devices.size() == 1);
        REQUIRE(devices[0]->id == device.id);
        REQUIRE(node.remove_device(&device));
        REQUIRE(node.get_devices().empty());
    }

    SECTION("Removing a device also removes dependent resources") {
        static constexpr uint32_t k_num_devices = 2;
        static constexpr uint32_t k_num_sources_per_device = 2;
        static constexpr uint32_t k_num_senders_per_source = 2;
        static constexpr uint32_t k_num_receivers_per_device = 2;

        uint32_t device_count = 0;
        uint32_t source_count = 0;
        uint32_t sender_count = 0;
        uint32_t receiver_count = 0;
        uint32_t flow_count = 0;

        boost::asio::io_context io_context;
        rav::ptp::Instance ptp_instance(io_context);
        rav::nmos::Node node(io_context, ptp_instance);

        std::vector<std::unique_ptr<rav::nmos::Device>> devices{};
        std::vector<std::unique_ptr<rav::nmos::SourceAudio>> sources{};
        std::vector<std::unique_ptr<rav::nmos::FlowAudioRaw>> flows{};
        std::vector<std::unique_ptr<rav::nmos::Sender>> senders{};
        std::vector<std::unique_ptr<rav::nmos::ReceiverAudio>> receivers{};

        // Devices
        for (uint32_t i_device = 0; i_device < k_num_devices; ++i_device) {
            auto& device = devices.emplace_back(std::make_unique<rav::nmos::Device>());
            device->id = boost::uuids::random_generator()();
            device->id = boost::uuids::random_generator()();
            device->label = fmt::format("ravennakit/device/{}", device_count);
            device->description = fmt::format("RAVENNAKIT Device {}", device_count + 1);
            REQUIRE(node.add_or_update_device(device.get()));

            // Sources
            for (uint32_t i_source = 0; i_source < k_num_sources_per_device; ++i_source) {
                auto& source = sources.emplace_back(std::make_unique<rav::nmos::SourceAudio>());
                source->id = boost::uuids::random_generator()();
                source->label = fmt::format("ravennakit/device/{}/source/{}", device_count, source_count);
                source->description = fmt::format("RAVENNAKIT Device {} source {}", device_count + 1, source_count + 1);
                source->device_id = device->id;
                source->channels.push_back({"Channel 1"});
                REQUIRE(node.add_or_update_source(source.get()));

                // Flow
                for (uint32_t i_sender = 0; i_sender < k_num_senders_per_source; ++i_sender) {
                    auto& flow = flows.emplace_back(std::make_unique<rav::nmos::FlowAudioRaw>());
                    flow->id = boost::uuids::random_generator()();
                    flow->label = fmt::format("ravennakit/device/{}/flow/{}", device_count, flow_count);
                    flow->description = fmt::format("RAVENNAKIT Device {} flow {}", device_count + 1, flow_count + 1);
                    flow->version = rav::nmos::Version {i_sender + 1, (i_sender + 1) * 1000};
                    flow->bit_depth = 24;
                    flow->sample_rate = {48000, 1};
                    flow->media_type = "audio/L24";
                    flow->source_id = source->id;
                    flow->device_id = device->id;
                    REQUIRE(node.add_or_update_flow(flow.get()));

                    // Sender
                    auto& sender = senders.emplace_back(std::make_unique<rav::nmos::Sender>());
                    sender->id = boost::uuids::random_generator()();
                    sender->label = fmt::format("ravennakit/device/{}/sender/{}", device_count, sender_count);
                    sender->description =
                        fmt::format("RAVENNAKIT Device {} sender {}", device_count + 1, sender_count + 1);
                    sender->version = rav::nmos::Version {i_sender + 1, (i_sender + 1) * 1000};
                    sender->device_id = device->id;
                    sender->transport = "urn:x-nmos:transport:rtp";
                    sender->flow_id = flow->id;
                    REQUIRE(node.add_or_update_sender(sender.get()));

                    flow_count++;
                    sender_count++;
                }

                source_count++;
            }

            // Receivers
            for (uint32_t i_receiver = 0; i_receiver < k_num_receivers_per_device; ++i_receiver) {
                auto& receiver = receivers.emplace_back(std::make_unique<rav::nmos::ReceiverAudio>());
                receiver->id = boost::uuids::random_generator()();
                receiver->label = fmt::format("ravennakit/device/{}/receiver/{}", device_count, receiver_count);
                receiver->description =
                    fmt::format("RAVENNAKIT Device {} sender {}", device_count + 1, receiver_count + 1);
                receiver->version = rav::nmos::Version {i_receiver + 1, (i_receiver + 1) * 1000};
                receiver->device_id = device->id;
                receiver->transport = "urn:x-nmos:transport:rtp";
                receiver->caps.media_types = {"audio/L24", "audio/L20", "audio/L16", "audio/L8", "audio/PCM"};
                REQUIRE(node.add_or_update_receiver(receiver.get()));

                receiver_count++;
            }
        }

        auto device_1_id = node.get_devices().front()->id;
        REQUIRE(node.remove_device(node.get_devices().front()));
        REQUIRE(node.get_devices().size() == k_num_devices - 1);
        REQUIRE(node.get_sources().size() == k_num_sources_per_device * (k_num_devices - 1));
        REQUIRE(node.get_flows().size() == k_num_senders_per_source * k_num_sources_per_device * (k_num_devices - 1));
        REQUIRE(node.get_senders().size() == k_num_senders_per_source * k_num_sources_per_device * (k_num_devices - 1));
        REQUIRE(node.get_receivers().size() == k_num_receivers_per_device * (k_num_devices - 1));

        for (auto* device : node.get_devices()) {
            REQUIRE(device->id != device_1_id);
        }

        for (auto& source : node.get_sources()) {
            REQUIRE(source->device_id != device_1_id);
        }

        for (auto& flow : node.get_flows()) {
            REQUIRE(flow->device_id != device_1_id);
        }

        for (auto& sender : node.get_senders()) {
            REQUIRE(sender->device_id != device_1_id);
        }

        for (auto& receiver : node.get_receivers()) {
            REQUIRE(receiver->device_id != device_1_id);
        }
    }

    SECTION("JSON") {
        rav::nmos::Node::Configuration config;
        config.id = boost::uuids::random_generator()();
        config.operation_mode = rav::nmos::OperationMode::mdns_p2p;
        config.api_version = rav::nmos::ApiVersion {1, 2};
        config.registry_address = "http://127.0.0.1:8080";
        config.enabled = false;
        config.api_port = 1234;
        config.label = "label";
        config.description = "description";

        rav::nmos::test_nmos_node_configuration_json(config, config.to_json());
        rav::nmos::test_nmos_node_configuration_json(config, boost::json::value_from(config));

        boost::asio::io_context io_context;
        rav::ptp::Instance ptp_instance(io_context);
        rav::nmos::Node node(io_context, ptp_instance);
        REQUIRE(node.set_configuration(config));
    }
}

void rav::nmos::test_nmos_node_configuration_json(const Node::Configuration& config, const boost::json::value& json) {
    REQUIRE(json.is_object());
    REQUIRE(json.at("id").as_string() == boost::uuids::to_string(config.id));
    REQUIRE(json.at("operation_mode").as_string() == rav::nmos::to_string(config.operation_mode));
    REQUIRE(json.at("api_version").as_string() == config.api_version.to_string());
    REQUIRE(json.at("registry_address").as_string() == config.registry_address);
    REQUIRE(json.at("enabled") == config.enabled);
    REQUIRE(json.at("api_port") == config.api_port);
    REQUIRE(json.at("label").as_string() == config.label);
    REQUIRE(json.at("description").as_string() == config.description);
}
