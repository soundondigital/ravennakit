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

#include "ravennakit/core/rollback.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/nmos/models/nmos_api_error.hpp"
#include "ravennakit/nmos/models/nmos_self.hpp"

#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/lexical_cast.hpp>
#include <utility>

namespace {

namespace http = boost::beast::http;

/**
 * Sets the default headers for the response.
 * Warning: these headers are probably not suitable for production use, see:
 * https://specs.amwa.tv/is-04/releases/v1.3.3/docs/APIs_-_Server_Side_Implementation_Notes.html#cross-origin-resource-sharing-cors
 */
void set_default_headers(rav::HttpServer::Response& res) {
    res.set("Content-Type", "application/json");
    res.set("Access-Control-Allow-Origin", "*");
    res.set("Access-Control-Allow-Methods", "GET, PUT, POST, PATCH, HEAD, OPTIONS, DELETE");
    res.set("Access-Control-Allow-Headers", "Content-Type, Accept");
    res.set("Access-Control-Max-Age", "3600");
}

/**
 * Sets the error response with the given status, error message, and debug information.
 * @param res The response to set.
 * @param status The HTTP status code.
 * @param error The error message.
 * @param debug The debug information.
 */
void set_error_response(
    http::response<http::string_body>& res, const http::status status, const std::string& error,
    const std::string& debug
) {
    res.result(status);
    set_default_headers(res);
    res.body() = boost::json::serialize(
        boost::json::value_from(rav::nmos::ApiError {static_cast<unsigned>(status), error, debug})
    );
    res.prepare_payload();
}

/**
 * Sets the error response for an invalid API version.
 * @param res The response to set.
 */
void invalid_api_version_response(http::response<http::string_body>& res) {
    set_error_response(
        res, http::status::bad_request, "Invalid API version",
        "Failed to parse a valid version in the form of vMAJOR.MINOR"
    );
}

/**
 * Sets the error response for an unsupported API version.
 * @param res The response to set.
 * @param version The unsupported API version.
 */
void version_not_supported_response(http::response<http::string_body>& res, const rav::nmos::ApiVersion& version) {
    set_error_response(
        res, http::status::not_found, "Version not found", fmt::format("Version {} is not supported", version)
    );
}

/**
 * Sets the response to indicate that the request was successful and optionally adds the body (if not empty).
 * @param res The response to set.
 * @param body The body of the response.
 */
void ok_response(http::response<http::string_body>& res, std::string body) {
    res.result(http::status::ok);
    set_default_headers(res);
    res.body() = std::move(body);
    res.prepare_payload();
}

/**
 * Checks if the given API version is supported.
 * @param version The API version to check.
 * @return True if the version is supported, false otherwise.
 */
bool is_version_supported(const rav::nmos::ApiVersion& version) {
    return std::any_of(
        rav::nmos::Node::k_supported_api_versions.begin(), rav::nmos::Node::k_supported_api_versions.end(),
        [&version](const auto& supported_version) {
            return supported_version == version;
        }
    );
}

/**
 * Gets the valid API version from the request parameters.
 * @param res The response to set.
 * @param params The request parameters.
 * @param param_name The name of the parameter to check (default: "version").
 * @return The valid API version, or std::nullopt if not found or invalid.
 */
std::optional<rav::nmos::ApiVersion> get_valid_version_from_parameters(
    rav::HttpServer::Response& res, const rav::PathMatcher::Parameters& params,
    const std::string_view param_name = "version"
) {
    const auto version_str = params.get(param_name);
    if (version_str == nullptr) {
        invalid_api_version_response(res);
        return std::nullopt;
    }
    const auto version = rav::nmos::ApiVersion::from_string(*version_str);
    if (!version) {
        invalid_api_version_response(res);
        return std::nullopt;
    }
    if (!is_version_supported(*version)) {
        version_not_supported_response(res, *version);
        return std::nullopt;
    }
    return version;
}

}  // namespace

std::array<rav::nmos::ApiVersion, 2> rav::nmos::Node::k_supported_api_versions = {{
    ApiVersion::v1_2(),
    ApiVersion::v1_3(),
}};

boost::system::result<void, rav::nmos::Error> rav::nmos::Node::Configuration::validate() const {
    bool version_valid = false;

    for (auto& v : k_supported_api_versions) {
        if (v == api_version) {
            version_valid = true;
            break;
        }
    }

    if (!version_valid) {
        return Error::invalid_api_version;
    }

    if (operation_mode == OperationMode::manual) {
        if (registry_address.empty()) {
            return Error::invalid_registry_address;
        }
    }

    return {};
}

void rav::nmos::Node::ConfigurationUpdate::apply_to_config(Configuration& config) const {
    if (operation_mode.has_value()) {
        config.operation_mode = *operation_mode;
    }
    if (api_version.has_value()) {
        config.api_version = *api_version;
    }
    if (registry_address.has_value()) {
        config.registry_address = *registry_address;
    }
    if (enabled.has_value()) {
        config.enabled = *enabled;
    }
    if (node_api_port.has_value()) {
        config.node_api_port = *node_api_port;
    }
}

rav::nmos::Node::Node(
    boost::asio::io_context& io_context, std::unique_ptr<RegistryBrowserBase> registry_browser,
    std::unique_ptr<HttpClientBase> http_client
) :
    http_server_(io_context),
    http_client_(std::move(http_client)),
    registry_browser_(std::move(registry_browser)),
    timer_(io_context),
    heartbeat_timer_(io_context) {
    if (!http_client_) {
        http_client_ = std::make_unique<HttpClient>(io_context);
    }
    if (!registry_browser_) {
        registry_browser_ = std::make_unique<RegistryBrowser>(io_context);
    }
    self_.id = boost::uuids::random_generator()();
    self_.version = Version {1, 0};
    self_.label = "ravennakit";
    self_.description = "RAVENNAKIT NMOS Node";
    self_.interfaces = {Self::Interface {std::nullopt, "00-1a-2b-3c-4d-5e", "eth0"}};

    auto local_clock = ClockInternal {};
    local_clock.name = "clk01";

    auto ptp_clock = ClockPtp {};
    ptp_clock.name = "clk02";
    ptp_clock.gmid = "00-1a-2b-00-00-3c-4d-5e";

    self_.clocks.emplace_back(local_clock);
    self_.clocks.emplace_back(ptp_clock);

    for (auto& v : k_supported_api_versions) {
        self_.api.versions.push_back(v.to_string());
    }

    http_server_.get("/", [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        ok_response(res, boost::json::serialize(boost::json::array({"x-nmos/"})));
    });

    http_server_.get("/x-nmos", [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        ok_response(res, boost::json::serialize(boost::json::array({"node/"})));
    });

    http_server_.get(
        "/x-nmos/node",
        [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
            res.result(boost::beast::http::status::ok);
            set_default_headers(res);

            boost::json::array versions;
            for (const auto& version : k_supported_api_versions) {
                versions.push_back({fmt::format("{}/", version.to_string())});
            }

            res.body() = boost::json::serialize(versions);
            res.prepare_payload();
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            ok_response(
                res,
                boost::json::serialize(
                    boost::json::array({"self/", "sources/", "flows/", "devices/", "senders/", "receivers/"})
                )
            );
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/self",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            ok_response(res, boost::json::serialize(boost::json::value_from(self_)));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/devices",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            ok_response(res, boost::json::serialize(boost::json::value_from(devices_)));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/devices/{device_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            const auto* uuid_str = params.get("device_id");
            if (uuid_str == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid device ID", "Device ID is empty");
                return;
            }

            const auto uuid = boost::lexical_cast<boost::uuids::uuid>(*uuid_str);
            if (uuid.is_nil()) {
                set_error_response(
                    res, http::status::bad_request, "Invalid device ID", "Device ID is not a valid UUID"
                );
                return;
            }

            if (auto* device = find_device(uuid)) {
                ok_response(res, boost::json::serialize(boost::json::value_from(*device)));
                return;
            }

            set_error_response(res, http::status::not_found, "Not found", "Device not found");
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/flows",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            ok_response(res, boost::json::serialize(boost::json::value_from(flows_)));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/flows/{flow_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            const auto* uuid_str = params.get("flow_id");
            if (uuid_str == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid flow ID", "Flow ID is empty");
                return;
            }

            const auto uuid = boost::lexical_cast<boost::uuids::uuid>(*uuid_str);
            if (uuid.is_nil()) {
                set_error_response(res, http::status::bad_request, "Invalid flow ID", "Flow ID is not a valid UUID");
                return;
            }

            if (auto* flow = find_flow(uuid)) {
                ok_response(res, boost::json::serialize(boost::json::value_from(*flow)));
                return;
            }

            set_error_response(res, http::status::not_found, "Not found", "Flow not found");
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/receivers",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            ok_response(res, boost::json::serialize(boost::json::value_from(receivers_)));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/receivers/{receiver_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            const auto* uuid_str = params.get("receiver_id");
            if (uuid_str == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "Receiver ID is empty");
                return;
            }

            const auto uuid = boost::lexical_cast<boost::uuids::uuid>(*uuid_str);
            if (uuid.is_nil()) {
                set_error_response(
                    res, http::status::bad_request, "Invalid receiver ID", "Receiver ID is not a valid UUID"
                );
                return;
            }

            if (auto* receiver = find_receiver(uuid)) {
                ok_response(res, boost::json::serialize(boost::json::value_from(*receiver)));
                return;
            }

            set_error_response(res, http::status::not_found, "Not found", "Receiver not found");
        }
    );

    http_server_.options(
        "/x-nmos/node/{version}/receivers/{receiver_id}/target",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            const auto* uuid_str = params.get("receiver_id");
            if (uuid_str == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "Receiver ID is empty");
                return;
            }

            const auto uuid = boost::lexical_cast<boost::uuids::uuid>(*uuid_str);
            if (uuid.is_nil()) {
                set_error_response(
                    res, http::status::bad_request, "Invalid receiver ID", "Receiver ID is not a valid UUID"
                );
                return;
            }

            ok_response(res, {});
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/senders",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            ok_response(res, boost::json::serialize(boost::json::value_from(senders_)));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/senders/{sender_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            const auto* uuid_str = params.get("sender_id");
            if (uuid_str == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "Sender ID is empty");
                return;
            }

            const auto uuid = boost::lexical_cast<boost::uuids::uuid>(*uuid_str);
            if (uuid.is_nil()) {
                set_error_response(
                    res, http::status::bad_request, "Invalid sender ID", "Sender ID is not a valid UUID"
                );
                return;
            }

            if (auto* sender = find_sender(uuid)) {
                ok_response(res, boost::json::serialize(boost::json::value_from(*sender)));
                return;
            }

            set_error_response(res, http::status::not_found, "Not found", "Sender not found");
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/sources",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            ok_response(res, boost::json::serialize(boost::json::value_from(sources_)));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/sources/{source_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_version_from_parameters(res, params)) {
                return;
            }

            const auto* uuid_str = params.get("source_id");
            if (uuid_str == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid source ID", "Source ID is empty");
                return;
            }

            const auto uuid = boost::lexical_cast<boost::uuids::uuid>(*uuid_str);
            if (uuid.is_nil()) {
                set_error_response(
                    res, http::status::bad_request, "Invalid source ID", "Source ID is not a valid UUID"
                );
                return;
            }

            if (auto* sender = find_source(uuid)) {
                ok_response(res, boost::json::serialize(boost::json::value_from(*sender)));
                return;
            }

            set_error_response(res, http::status::not_found, "Not found", "Source not found");
        }
    );

    http_server_.get("/**", [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        set_error_response(res, http::status::not_found, "Not found", "No matching route");
    });
}

boost::system::result<void, rav::nmos::Error> rav::nmos::Node::start() {
    configuration_.enabled = true;
    return start_internal();
}

void rav::nmos::Node::stop() {
    configuration_.enabled = false;
    stop_internal();
}

boost::system::result<void, rav::nmos::Error>
rav::nmos::Node::update_configuration(const ConfigurationUpdate& update, const bool force_update) {
    auto new_config = configuration_;
    update.apply_to_config(new_config);

    if (new_config == configuration_ && !force_update) {
        return {};  // Nothing changed, so we should be in the correct state.
    }

    auto result = new_config.validate();
    if (result.has_error()) {
        return result;
    }

    configuration_ = std::move(new_config);

    stop_internal();

    if (configuration_.enabled) {
        result = start_internal();
        if (result.has_error()) {
            return result;
        }
    }

    return {};
}

const rav::nmos::Node::Configuration& rav::nmos::Node::get_configuration() const {
    return configuration_;
}

boost::system::result<void, rav::nmos::Error> rav::nmos::Node::start_internal() {
    const auto result = http_server_.start("0.0.0.0", configuration_.node_api_port);
    if (result.has_error()) {
        RAV_ERROR("Failed to start HTTP server: {}", result.error().message());
        return Error::failed_to_start_http_server;
    }

    // TODO: I'm not sure if this is the right value for href, but unless a web interface is served this is the best we
    // got.
    const auto endpoint = http_server_.get_local_endpoint();
    self_.href = fmt::format(
        "http://{}:{}/x-nmos/node/{}", endpoint.address().to_string(), endpoint.port(),
        k_supported_api_versions.back().to_string()
    );

    self_.api.endpoints = {Self::Endpoint {endpoint.address().to_string(), endpoint.port(), "http", false}};

    // Start the HTTP client to connect to the registry.
    if (configuration_.operation_mode == OperationMode::manual) {
        if (configuration_.registry_address.empty()) {
            RAV_ERROR("Registry address is empty");
            return Error::invalid_registry_address;
        }

        auto url = boost::urls::parse_uri_reference(configuration_.registry_address);
        if (url.has_error()) {
            RAV_ERROR(
                "Invalid registry address: {} (should be in the form of: scheme://host:port)",
                configuration_.registry_address
            );
            return Error::invalid_registry_address;
        }
        connect_to_registry_async(url->host(), url->port());
        return {};
    }

    if (configuration_.operation_mode == OperationMode::p2p) {
        selected_registry_.reset();
        registry_browser_->stop();
        update_status({});
        return {};
    }

    // All other cases require a timeout to wait for the registry to be discovered

    registry_browser_->on_registry_discovered.reset();
    registry_browser_->start(configuration_.operation_mode, configuration_.api_version);

    timer_.once(k_default_timeout, [this] {
        // Subscribe to future registry discoveries
        registry_browser_->on_registry_discovered = [this](const dnssd::ServiceDescription& desc) {
            handle_registry_discovered(desc);
        };
        if (const auto reg = registry_browser_->find_most_suitable_registry()) {
            select_registry(*reg);
        } else {
            update_status({});
        }
    });

    return {};
}

void rav::nmos::Node::stop_internal() {
    RAV_ASSERT(http_client_ != nullptr, "HTTP client should not be null");

    heartbeat_timer_.stop();
    timer_.stop();
    http_client_->cancel_outstanding_requests();
    http_server_.stop();
    RAV_ASSERT(registry_browser_ != nullptr, "Registry browser should not be null");
    registry_browser_->stop();
}

void rav::nmos::Node::register_async() {
    post_resource_async("node", boost::json::value_from(self_));

    for (auto& device : devices_) {
        post_resource_async("device", boost::json::value_from(device));
    }

    for (auto& source : sources_) {
        post_resource_async("source", boost::json::value_from(source));
    }

    for (auto& flow : flows_) {
        post_resource_async("flow", boost::json::value_from(flow));
    }

    for (auto& sender : senders_) {
        post_resource_async("sender", boost::json::value_from(sender));
    }

    for (auto& receiver : receivers_) {
        post_resource_async("receiver", boost::json::value_from(receiver));
    }

    failed_heartbeat_count_ = 0;

    // Send heartbeat immediately after registration to update the connected status.
    send_heartbeat_async();

    heartbeat_timer_.start(k_heartbeat_interval, [this] {
        send_heartbeat_async();
    });
}

void rav::nmos::Node::post_resource_async(std::string type, boost::json::value resource) const {
    RAV_ASSERT(http_client_ != nullptr, "HTTP client should not be null");

    const auto target = fmt::format("/x-nmos/registration/{}/resource", configuration_.api_version.to_string());

    auto body = boost::json::serialize(boost::json::value {{"type", type}, {"data", std::move(resource)}});

    http_client_->post_async(
        target, body,
        [body, type](const boost::system::result<http::response<http::string_body>>& result) mutable {
            if (result.has_error()) {
                RAV_ERROR("Failed to register with registry: {}", result.error().message());
                return;
            }

            const auto& res = result.value();

            if (res.result() == http::status::ok) {
                RAV_INFO("Updated {} in registry: {}", type, body);
            } else if (res.result() == http::status::created) {
                RAV_INFO("Created {} in registry: {}", type, body);
            } else if (http::to_status_class(res.result()) == http::status_class::successful) {
                RAV_WARNING("Unexpected response from registry: {}", res.result_int());
            } else {
                RAV_ERROR("Failed to post resource {}: {}", res, body);
            }
        },
        {}
    );
}

void rav::nmos::Node::send_heartbeat_async() {
    const auto target = fmt::format(
        "/x-nmos/registration/{}/health/nodes/{}", configuration_.api_version.to_string(), to_string(self_.id)
    );

    http_client_->post_async(
        target, {},
        [this](const boost::system::result<http::response<http::string_body>>& result) {
            if (result.has_value() && result.value().result() == http::status::ok) {
                failed_heartbeat_count_ = 0;
                if (!std::exchange(is_registered_, true)) {
                    RAV_INFO("Node registered with the registry");
                }
                return;
            }
            failed_heartbeat_count_++;
            if (result.has_error()) {
                RAV_ERROR("Failed to send heartbeat: {}", result.error().message());
                // When this case happens, it's pretty reasonable to assume that the connection is lost.
            } else {
                RAV_ERROR("Sending heartbeat failed: {}", result->result_int());
                if (failed_heartbeat_count_ < k_max_failed_heartbeats) {
                    return;  // Don't try to reconnect yet, just try the next heartbeat.
                }
            }
            http_client_->cancel_outstanding_requests();
            RAV_ERROR("Failed to send heartbeat {} times, stopping heartbeat", failed_heartbeat_count_);
            is_registered_ = false;
            heartbeat_timer_.stop();
            connect_to_registry_async();
        },
        {}
    );
}

void rav::nmos::Node::connect_to_registry_async() {
    http_client_->get_async("/", [this](const boost::system::result<http::response<http::string_body>>& result) {
        if (result.has_error()) {
            RAV_INFO("Error connecting to NMOS registry: {}", result.error().message());
            Status state = {};
            state.error_message = fmt::format("Error connecting to NMOS registry: {}", result.error().message());
            update_status(state);
            timer_.once(k_default_timeout, [this] {
                connect_to_registry_async();  // Retry connection
            });
        } else if (result.value().result() != http::status::ok) {
            RAV_ERROR("Unexpected response from NMOS registry: {}", result.value().result_int());
            Status state;
            state.error_message =
                fmt::format("Unexpected response from NMOS registry: {}", result.value().result_int());
            update_status(state);
        } else {
            register_async();
            Status state;
            state.registered = true;
            update_status(state);
        }
    });
}

void rav::nmos::Node::connect_to_registry_async(const std::string_view host, const std::string_view service) {
    http_client_->set_host(host, service);
    connect_to_registry_async();
}

bool rav::nmos::Node::add_receiver_to_device(const Receiver& receiver) {
    for (auto& device : devices_) {
        if (device.id == receiver.device_id()) {
            device.receivers.push_back(receiver.id());
            return true;
        }
    }
    return false;
}

bool rav::nmos::Node::add_sender_to_device(const Sender& sender) {
    for (auto& device : devices_) {
        if (device.id == sender.device_id) {
            device.senders.push_back(sender.id);
            return true;
        }
    }
    return false;
}

bool rav::nmos::Node::select_registry(const dnssd::ServiceDescription& desc) {
    if (selected_registry_ && selected_registry_->host_target == desc.host_target
        && selected_registry_->port == desc.port) {
        return false;  // Already connected to this registry
    }
    selected_registry_ = desc;
    connect_to_registry_async(selected_registry_->host_target, std::to_string(selected_registry_->port));
    return true;  // Successfully selected a new registry
}

void rav::nmos::Node::handle_registry_discovered(const dnssd::ServiceDescription& desc) {
    RAV_INFO("Discovered NMOS registry: {}", desc.to_string());
    if (selected_registry_.has_value()) {
        RAV_TRACE("Already connected to a registry, ignoring discovery: {}", selected_registry_->to_string());
        return;
    }
    if (configuration_.operation_mode == OperationMode::mdns_p2p) {
        select_registry(desc);
    }
}

void rav::nmos::Node::update_status(const Status& new_status) {
    if (status_ != new_status) {
        status_ = new_status;
        on_status_changed(status_);
    }
}

boost::asio::ip::tcp::endpoint rav::nmos::Node::get_local_endpoint() const {
    return http_server_.get_local_endpoint();
}

bool rav::nmos::Node::add_or_update_device(Device device) {
    if (device.id.is_nil()) {
        RAV_ERROR("Device ID should not be nil");
        return false;
    }

    device.node_id = self_.id;

    for (auto& existing_device : devices_) {
        if (existing_device.id == device.id) {
            existing_device = std::move(device);
            return true;
        }
    }

    devices_.push_back(std::move(device));
    return true;
}

const rav::nmos::Device* rav::nmos::Node::find_device(boost::uuids::uuid uuid) const {
    const auto it = std::find_if(devices_.begin(), devices_.end(), [uuid](const Device& device) {
        return device.id == uuid;
    });
    if (it != devices_.end()) {
        return &*it;
    }
    return nullptr;
}

bool rav::nmos::Node::add_or_update_flow(Flow flow) {
    if (flow.id().is_nil()) {
        RAV_ERROR("Flow ID should not be nil");
        return false;
    }

    for (auto& existing_flow : flows_) {
        if (existing_flow.id() == flow.id()) {
            existing_flow = std::move(flow);
            return true;
        }
    }

    flows_.push_back(std::move(flow));
    return true;
}

const rav::nmos::Flow* rav::nmos::Node::find_flow(boost::uuids::uuid uuid) const {
    const auto it = std::find_if(flows_.begin(), flows_.end(), [uuid](const Flow& flow) {
        return flow.id() == uuid;
    });
    if (it != flows_.end()) {
        return &*it;
    }
    return nullptr;
}

bool rav::nmos::Node::add_or_update_receiver(Receiver receiver) {
    if (receiver.id().is_nil()) {
        RAV_ERROR("Flow ID should not be nil");
        return false;
    }

    for (auto& existing_receiver : receivers_) {
        if (existing_receiver.id() == receiver.id()) {
            existing_receiver = std::move(receiver);
            return true;
        }
    }

    if (!add_receiver_to_device(receiver)) {
        RAV_ERROR("Device not found");
        return false;
    }

    receivers_.push_back(std::move(receiver));
    return true;
}

const rav::nmos::Receiver* rav::nmos::Node::find_receiver(boost::uuids::uuid uuid) const {
    const auto it = std::find_if(receivers_.begin(), receivers_.end(), [uuid](const Receiver& receiver) {
        return receiver.id() == uuid;
    });
    if (it != receivers_.end()) {
        return &*it;
    }
    return nullptr;
}

bool rav::nmos::Node::add_or_update_sender(Sender sender) {
    if (sender.id.is_nil()) {
        RAV_ERROR("Flow ID should not be nil");
        return false;
    }

    for (auto& existing_sender : senders_) {
        if (existing_sender.id == sender.id) {
            existing_sender = std::move(sender);
            return true;
        }
    }

    if (!add_sender_to_device(sender)) {
        RAV_ERROR("Device not found");
        return false;
    }

    senders_.push_back(std::move(sender));
    return true;
}

const rav::nmos::Sender* rav::nmos::Node::find_sender(boost::uuids::uuid uuid) const {
    const auto it = std::find_if(senders_.begin(), senders_.end(), [uuid](const Sender& sender) {
        return sender.id == uuid;
    });
    if (it != senders_.end()) {
        return &*it;
    }
    return nullptr;
}

bool rav::nmos::Node::add_or_update_source(Source source) {
    if (source.id().is_nil()) {
        RAV_ERROR("Source ID should not be nil");
        return false;
    }

    for (auto& existing_source : sources_) {
        if (existing_source.id() == source.id()) {
            existing_source = std::move(source);
            return true;
        }
    }

    sources_.push_back(std::move(source));
    return true;
}

const rav::nmos::Source* rav::nmos::Node::find_source(boost::uuids::uuid uuid) const {
    const auto it = std::find_if(sources_.begin(), sources_.end(), [uuid](const Source& source) {
        return source.id() == uuid;
    });
    if (it != sources_.end()) {
        return &*it;
    }
    return nullptr;
}

const boost::uuids::uuid& rav::nmos::Node::get_uuid() const {
    return self_.id;
}

const std::vector<rav::nmos::Device>& rav::nmos::Node::get_devices() const {
    return devices_;
}

const rav::nmos::Node::Status& rav::nmos::Node::get_status() const {
    return status_;
}
