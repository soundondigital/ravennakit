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

#include "ravennakit/core/json.hpp"
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
            return Error::no_registry_address_given;
        }

        auto url = boost::urls::parse_uri_reference(registry_address);
        if (url.has_error()) {
            return Error::invalid_registry_address;
        }
        if (!url->scheme().empty() && url->scheme() != "http" && url->scheme() != "https") {
            return Error::invalid_registry_address;
        }
        if (url->host().empty()) {
            return Error::invalid_registry_address;
        }
    }

    return {};
}

boost::json::value rav::nmos::Node::Configuration::to_json() const {
    return {
        {"id", to_string(id)},
        {"operation_mode", to_string(operation_mode)},
        {"api_version", api_version.to_string()},
        {"registry_address", registry_address},
        {"enabled", enabled},
        {"node_api_port", node_api_port},
        {"label", label},
        {"description", description},
    };
}

void rav::nmos::Node::ConfigurationUpdate::apply_to_config(Configuration& config) const {
    if (id.has_value()) {
        config.id = *id;
    }
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
    if (label.has_value()) {
        config.label = *label;
    }
    if (description.has_value()) {
        config.description = *description;
    }
}

boost::system::result<rav::nmos::Node::ConfigurationUpdate, std::string>
rav::nmos::Node::ConfigurationUpdate::from_json(const boost::json::value& json) {
    try {
        ConfigurationUpdate update {};

        // UUID
        const auto& uuid_str = json.at("id").as_string();
        const auto uuid = boost::lexical_cast<boost::uuids::uuid>(std::string_view(uuid_str));
        if (uuid.is_nil()) {
            return {"Invalid UUID"};
        }
        update.id = uuid;

        // Operation mode
        const auto& operation_mode_str = json.at("operation_mode").as_string();
        const auto operation_mode = operation_mode_from_string(operation_mode_str);
        if (!operation_mode.has_value()) {
            return fmt::format("Invalid operation mode: {}", std::string_view(operation_mode_str.c_str()));
        }
        update.operation_mode = *operation_mode;

        // Api version
        const auto& api_version_str = json.at("api_version").as_string();
        const auto api_version = ApiVersion::from_string(api_version_str);
        if (!api_version) {
            return fmt::format("Invalid API version: {}", std::string_view(api_version_str));
        }
        update.api_version = *api_version;

        update.registry_address = json.at("registry_address").as_string();
        update.enabled = json.at("enabled").as_bool();
        update.node_api_port = json.at("node_api_port").to_number<uint16_t>();
        update.label = json.at("label").as_string();
        update.description = json.at("description").as_string();
        return update;
    } catch (const std::exception& e) {
        return e.what();
    }
}

rav::nmos::Node::Node(
    boost::asio::io_context& io_context, ptp::Instance& ptp_instance,
    std::unique_ptr<RegistryBrowserBase> registry_browser, std::unique_ptr<HttpClientBase> http_client
) :
    ptp_instance_(ptp_instance),
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

    configuration_.id = boost::uuids::random_generator()();
    self_.id = configuration_.id;

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
            res.result(http::status::ok);
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

    if (!ptp_instance_.subscribe(this)) {
        RAV_ERROR("Failed to subscribe to PTP instance");
    }
}

rav::nmos::Node::~Node() {
    if (status_ == Status::registered) {
        unregister_async();
    }
    if (!ptp_instance_.unsubscribe(this)) {
        RAV_ERROR("Failed to unsubscribe from PTP instance");
    }
}

boost::system::result<void, rav::nmos::Error> rav::nmos::Node::start() {
    configuration_.enabled = true;
    return start_internal();
}

void rav::nmos::Node::stop() {
    configuration_.enabled = false;
    stop_internal();
    unregister_async();
}

void rav::nmos::Node::update_configuration(const ConfigurationUpdate& update, const bool force_update) {
    auto new_config = configuration_;
    update.apply_to_config(new_config);

    if (new_config == configuration_ && !force_update) {
        return;  // Nothing changed, so we should be in the correct state.
    }

    bool unregister = false;

    if (status_ == Status::registered) {
        if (configuration_.api_version != new_config.api_version) {
            unregister = true;
        }

        if (configuration_.id != new_config.id) {
            unregister = true;
        }

        if (!new_config.enabled) {
            unregister = true;
        }
    }

    if (unregister) {
        unregister_async();  // Before configuration_ is overwritten
    }

    const bool enablement_changed = configuration_.enabled != new_config.enabled;
    const bool operation_mode_changed = configuration_.operation_mode != new_config.operation_mode;

    configuration_ = std::move(new_config);
    if (status_ == Status::registered) {
        update_self();
        send_updated_resources_async();
    }
    on_configuration_changed(configuration_);

    if (enablement_changed) {
        if (configuration_.enabled) {
            auto result = configuration_.validate();
            if (result.has_error()) {
                RAV_ERROR("Invalid configuration: {}", result.error());
                update_status(Status::error);
                return;
            }

            result = start_internal();
            if (result.has_error()) {
                RAV_ERROR("Failed to start NMOS node: {}", result.error());
                update_status(Status::error);
            }
            return;
        }

        stop_internal();
        update_status(Status::disabled);
        return;
    }

    if (operation_mode_changed) {
        stop_internal();
        const auto result = start_internal();
        if (result.has_error()) {
            RAV_ERROR("Failed to start NMOS node: {}", result.error());
            update_status(Status::error);
        }
    }
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

    const auto http_endpoint = http_server_.get_local_endpoint();
    for (auto& endpoint : self_.api.endpoints) {
        endpoint.port = http_endpoint.port();
    }

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
        registry_info_.name = "(custom registry)";
        const auto port = url->port().empty() ? (url->scheme() == "https" ? "443" : "80") : url->port();
        connect_to_registry_async(url->host(), port);
        return {};
    }

    if (configuration_.operation_mode == OperationMode::p2p) {
        selected_registry_.reset();
        registry_browser_->stop();
        update_status(Status::p2p);
        return {};
    }

    if (selected_registry_.has_value()) {
        connect_to_registry_async();
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
        } else if (configuration_.operation_mode == OperationMode::mdns_p2p) {
            update_status(Status::discovering);
        } else {
            update_status(Status::p2p);
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
    selected_registry_.reset();
    registry_info_ = {};
}

void rav::nmos::Node::register_async() {
    post_resource_error_count_ = 0;
    failed_heartbeat_count_ = 0;

    update_self();
    update_all_resources_to_now();
    send_updated_resources_async();

    http_client_->get_async("/", [this](const boost::system::result<http::response<http::string_body>>& result) {
        if (result.has_error()) {
            RAV_ERROR("Failed to connect to NMOS registry: {}", result.error().message());
            update_status(Status::error);
            return;
        }

        if (result.value().result() != http::status::ok) {
            RAV_ERROR("Unexpected response from NMOS registry: {}", result.value().result_int());
            update_status(Status::error);
            return;
        }

        if (post_resource_error_count_ > 0) {
            RAV_ERROR("Failed to post one or more resources to the NMOS registry");
            update_status(Status::error);
            return;
        }

        RAV_INFO("Registered with NMOS registry");
        update_status(Status::registered);

        heartbeat_timer_.start(k_heartbeat_interval, [this] {
            send_heartbeat_async();
        });
    });
}

void rav::nmos::Node::unregister_async() {
    delete_resource_async("nodes", self_.id);
}

void rav::nmos::Node::post_resource_async(std::string type, boost::json::value resource) {
    RAV_ASSERT(http_client_ != nullptr, "HTTP client should not be null");

    const auto target = fmt::format("/x-nmos/registration/{}/resource", configuration_.api_version.to_string());

    auto body = boost::json::serialize(boost::json::value {{"type", type}, {"data", std::move(resource)}});

    http_client_->post_async(
        target, body,
        [this, target, body](const boost::system::result<http::response<http::string_body>>& result) mutable {
            if (result.has_error()) {
                RAV_ERROR("Failed to register with registry: {}", result.error().message());
                post_resource_error_count_++;
                return;
            }

            const auto& res = result.value();

            if (res.result() == http::status::ok) {
                RAV_INFO("Updated {} {}", target, body);
            } else if (res.result() == http::status::created) {
                RAV_INFO("Created {} {}", target, body);
            } else if (http::to_status_class(res.result()) == http::status_class::successful) {
                RAV_WARNING("Unexpected response from registry: {}", res.result_int());
            } else {
                post_resource_error_count_++;
                if (const auto error = parse_json<ApiError>(res.body())) {
                    RAV_ERROR("Failed to post resource: {} ({}) {}", error->code, error->error, body);
                } else {
                    RAV_ERROR("Failed to post resource: {} ({}) {}", res.result_int(), res.body(), body);
                }
                update_status(Status::error);
            }
        },
        {}
    );
}

void rav::nmos::Node::delete_resource_async(std::string resource_type, const boost::uuids::uuid& id) {
    RAV_ASSERT(http_client_ != nullptr, "HTTP client should not be null");

    const auto target = fmt::format(
        "/x-nmos/registration/{}/resource/{}/{}", configuration_.api_version.to_string(), resource_type, to_string(id)
    );

    http_client_->delete_async(
        target,
        [this, target](const boost::system::result<http::response<http::string_body>>& result) mutable {
            if (result.has_error()) {
                RAV_ERROR("Failed to delete resource from registry: {}", result.error().message());
                return;
            }

            const auto& res = result.value();

            if (res.result() == http::status::no_content) {
                RAV_INFO("Deleted {}", target);
            } else if (http::to_status_class(res.result()) == http::status_class::successful) {
                RAV_WARNING("Unexpected response from registry: {}", res.result_int());
            } else {
                if (const auto error = parse_json<ApiError>(res.body())) {
                    RAV_ERROR("Failed to delete resource at: {} {} ({})", target, error->code, error->error);
                } else {
                    RAV_ERROR("Failed to delete resource at: {} {} ({})", target, res.result_int(), res.body());
                }
                update_status(Status::error);
            }
        }
    );
}

void rav::nmos::Node::update_self() {
    const auto now = get_local_clock().now();
    self_.version.update(now);
    self_.id = configuration_.id;
    self_.label = configuration_.label;
    self_.description = configuration_.description;
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
                return;
            }
            failed_heartbeat_count_++;
            if (result.has_error()) {
                RAV_ERROR("Failed to send heartbeat: {}", result.error().message());
                update_status(Status::error);
                // When this case happens, it's pretty reasonable to assume that the connection is lost.
            } else {
                RAV_ERROR("Sending heartbeat failed: {}", result->result_int());
                if (failed_heartbeat_count_ < k_max_failed_heartbeats) {
                    return;  // Don't try to reconnect yet, just try the next heartbeat.
                }
            }
            http_client_->cancel_outstanding_requests();
            RAV_ERROR("Failed to send heartbeat {} times, stopping heartbeat", failed_heartbeat_count_);
            update_status(Status::error);
            heartbeat_timer_.stop();
            connect_to_registry_async();
        },
        {}
    );
}

void rav::nmos::Node::connect_to_registry_async() {
    http_client_->get_async("/", [this](const boost::system::result<http::response<http::string_body>>& result) {
        if (result.has_error()) {
            RAV_TRACE("Error connecting to NMOS registry: {}", result.error().message());
            update_status(Status::error);
            timer_.once(k_default_timeout, [this] {
                connect_to_registry_async();  // Retry connection
            });
        } else if (result.value().result() != http::status::ok) {
            RAV_ERROR("Unexpected response from NMOS registry: {}", result.value().result_int());
            update_status(Status::error);
        } else {
            register_async();
            update_status(Status::connected);
        }
    });
}

void rav::nmos::Node::connect_to_registry_async(const std::string_view host, const std::string_view service) {
    http_client_->set_host(host, service);
    registry_info_.address = fmt::format("http://{}:{}", host, service);
    update_status(Status::connecting);
    connect_to_registry_async();
}

bool rav::nmos::Node::add_receiver_to_device(const Receiver& receiver) {
    for (auto& device : devices_) {
        if (device.id == receiver.get_device_id()) {
            device.receivers.push_back(receiver.get_id());
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
        && selected_registry_->port == desc.port && status_ == Status::registered) {
        return false;  // Already connected to this registry
    }
    selected_registry_ = desc;
    registry_info_.name = desc.name;
    connect_to_registry_async(selected_registry_->host_target, std::to_string(selected_registry_->port));
    return true;  // Successfully selected a new registry
}

void rav::nmos::Node::handle_registry_discovered(const dnssd::ServiceDescription& desc) {
    RAV_INFO("Discovered NMOS registry: {}", desc.to_string());
    if (selected_registry_.has_value()) {
        RAV_TRACE("Ignoring discovery: already connected to a registry: {}", selected_registry_->to_string());
        return;
    }
    if (configuration_.operation_mode == OperationMode::mdns_p2p) {
        select_registry(desc);
    }
}

void rav::nmos::Node::update_status(const Status new_status) {
    if (status_ == new_status) {
        return;  // No change in status, nothing to do.
    }
    status_ = new_status;
    if (status_ == Status::disabled) {
        selected_registry_.reset();
    }
    on_status_changed(status_, registry_info_);
}

void rav::nmos::Node::update_all_resources_to_now() {
    const auto version = Version(get_local_clock().now());

    self_.version = version;

    for (auto& device : devices_) {
        device.version = version;
    }

    for (auto& source : sources_) {
        source.set_version(version);
    }

    for (auto& flow : flows_) {
        flow.set_version(version);
    }

    for (auto& sender : senders_) {
        sender.version = version;
    }

    for (auto& receiver : receivers_) {
        receiver.set_version(version);
    }
}

void rav::nmos::Node::send_updated_resources_async() {
    auto new_version = Version(get_local_clock().now());

    if (self_.version > current_version_) {
        post_resource_async("node", boost::json::value_from(self_));
    }

    for (auto& device : devices_) {
        if (device.version > current_version_) {
            post_resource_async("device", boost::json::value_from(device));
        }
    }

    for (auto& source : sources_) {
        if (source.get_version() > current_version_) {
            post_resource_async("source", boost::json::value_from(source));
        }
    }

    for (auto& flow : flows_) {
        if (flow.get_version() > current_version_) {
            post_resource_async("flow", boost::json::value_from(flow));
        }
    }

    for (auto& sender : senders_) {
        if (sender.version > current_version_) {
            post_resource_async("sender", boost::json::value_from(sender));
        }
    }

    for (auto& receiver : receivers_) {
        if (receiver.get_version() > current_version_) {
            post_resource_async("receiver", boost::json::value_from(receiver));
        }
    }

    current_version_ = Version(new_version);
}

const char* rav::nmos::to_string(const Node::Status& status) {
    switch (status) {
        case Node::Status::disabled:
            return "disabled";
        case Node::Status::connecting:
            return "connecting";
        case Node::Status::connected:
            return "connected";
        case Node::Status::registered:
            return "registered";
        case Node::Status::p2p:
            return "p2p";
        case Node::Status::error:
            return "error";
        case Node::Status::discovering:
            return "discovering";
    }
    return "unknown";
}

boost::asio::ip::tcp::endpoint rav::nmos::Node::get_local_endpoint() const {
    return http_server_.get_local_endpoint();
}

bool rav::nmos::Node::add_or_update_device(Device device) {
    RAV_ASSERT(!device.id.is_nil(), "Device ID should not be nil");
    RAV_ASSERT(!self_.id.is_nil(), "Node ID should not be nil");

    device.node_id = self_.id;
    device.version.update(get_local_clock().now());

    bool updated = false;
    for (auto& existing_device : devices_) {
        if (existing_device.id == device.id) {
            existing_device = std::move(device);
            updated = true;
            break;
        }
    }

    if (!updated) {
        devices_.push_back(std::move(device));
    }

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }

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
    if (receiver.get_id().is_nil()) {
        RAV_ERROR("Flow ID should not be nil");
        return false;
    }

    for (auto& existing_receiver : receivers_) {
        if (existing_receiver.get_id() == receiver.get_id()) {
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
        return receiver.get_id() == uuid;
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
    if (source.get_id().is_nil()) {
        RAV_ERROR("Source ID should not be nil");
        return false;
    }

    for (auto& existing_source : sources_) {
        if (existing_source.get_id() == source.get_id()) {
            existing_source = std::move(source);
            return true;
        }
    }

    sources_.push_back(std::move(source));
    return true;
}

const rav::nmos::Source* rav::nmos::Node::find_source(boost::uuids::uuid uuid) const {
    const auto it = std::find_if(sources_.begin(), sources_.end(), [uuid](const Source& source) {
        return source.get_id() == uuid;
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

const rav::nmos::Node::RegistryInfo& rav::nmos::Node::get_registry_info() const {
    return registry_info_;
}

boost::json::value rav::nmos::Node::to_json() const {
    return {
        {"configuration", configuration_.to_json()},        {"devices", boost::json::value_from(devices_)},
        {"flows", boost::json::value_from(flows_)},         {"senders", boost::json::value_from(senders_)},
        {"receivers", boost::json::value_from(receivers_)}, {"sources", boost::json::value_from(sources_)},
    };
}

boost::system::result<void, std::string> rav::nmos::Node::restore_from_json(const boost::json::value& json) {
    const auto config = json.try_at("configuration");
    if (!config || !config->is_object()) {
        return "invalid configuration JSON";
    }

    auto update = ConfigurationUpdate::from_json(*config);
    if (update.has_error()) {
        return update.error();
    }

    const auto devices_json = json.try_at("devices");
    std::vector<Device> devices;
    if (devices_json && devices_json->is_array()) {
        devices = boost::json::value_to<std::vector<Device>>(*devices_json);
    }

    update_configuration(*update);
    devices_ = std::move(devices);

    return {};
}

void rav::nmos::Node::set_network_interface_config(const NetworkInterfaceConfig& interface_config) {
    auto addrs = interface_config.get_interface_ipv4_addresses();
    if (addrs.empty()) {
        RAV_ERROR("No IPv4 addresses found for the interface");
        return;
    }

    const auto http_endpoint = http_server_.get_local_endpoint();

    self_.api.endpoints.clear();

    for (const auto& [rank, ip] : addrs) {
        self_.api.endpoints.emplace_back(Self::Endpoint {ip.to_string(), http_endpoint.port(), "http", false});
    }

    self_.version.update(get_local_clock().now());

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }
}

std::optional<size_t> rav::nmos::Node::index_of_supported_api_version(const ApiVersion& version) {
    const auto it = std::find(k_supported_api_versions.begin(), k_supported_api_versions.end(), version);
    if (it != k_supported_api_versions.end()) {
        return std::distance(k_supported_api_versions.begin(), it);
    }
    return std::nullopt;
}

void rav::nmos::Node::ptp_parent_changed(const ptp::ParentDs& parent) {
    ClockPtp clock;
    clock.gmid = parent.grandmaster_identity.to_string();
    clock.name = "clk0";
    clock.traceable = ptp_instance_.get_time_properties_ds().time_traceable;
    self_.clocks = {clock};
    self_.version.update(get_local_clock().now());

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }
}

void rav::nmos::Node::ptp_port_changed_state(const ptp::Port&) {
    if (self_.clocks.empty()) {
        RAV_ERROR("No clocks available to update PTP state");
        return;
    }

    auto locked = get_local_clock().is_locked();

    const auto changed = std::visit(
        [locked](auto& clock) {
            if constexpr (std::is_same_v<ClockPtp, std::decay_t<decltype(clock)>>) {
                if (clock.locked == locked) {
                    return false;
                }
                clock.locked = locked;
                return true;
            } else {
                RAV_ERROR("Unsupported clock type for PTP port state change");
            }

            return false;
        },
        self_.clocks.front()
    );

    if (changed) {
        self_.version.update(get_local_clock().now());
        if (status_ == Status::registered) {
            send_updated_resources_async();
        }
    }
}
