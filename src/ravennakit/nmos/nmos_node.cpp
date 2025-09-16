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
#include "ravennakit/core/util/defer.hpp"
#include "ravennakit/core/util/stl_helpers.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/nmos/models/nmos_activation_response.hpp"
#include "ravennakit/nmos/models/nmos_api_error.hpp"
#include "ravennakit/nmos/models/nmos_constraints_rtp.hpp"
#include "ravennakit/nmos/models/nmos_receiver_transport_params_rtp.hpp"
#include "ravennakit/nmos/models/nmos_self.hpp"
#include "ravennakit/nmos/models/nmos_sender_transport_params_rtp.hpp"
#include "ravennakit/nmos/models/nmos_transport_file.hpp"
#include "ravennakit/nmos/detail/nmos_uuid.hpp"
#include "ravennakit/nmos/models/nmos_activation.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/range.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

#include <utility>

namespace {

namespace http = boost::beast::http;

/**
 * Sets the default headers for the response.
 * Warning: these headers are probably not suitable for production use, see:
 * https://specs.amwa.tv/is-04/releases/v1.3.3/docs/APIs_-_Server_Side_Implementation_Notes.html#cross-origin-resource-sharing-cors
 */
void set_default_headers(rav::HttpServer::Response& res, const char* content_type = "application/json") {
    res.set("Content-Type", content_type);
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
    set_default_headers(res, "application/json");
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
 * Sets the response to indicate that the request was successful and optionally adds the body (if not empty).
 * @param res The response to set.
 * @param body The body of the response.
 * @param content_type The mime type of the content.
 */
void ok_response(
    http::response<http::string_body>& res, std::string body, const char* content_type = "application/json"
) {
    res.result(http::status::ok);
    set_default_headers(res, content_type);
    res.body() = std::move(body);
    res.prepare_payload();
}

template<typename VersionsContainer>
std::optional<rav::nmos::ApiVersion> get_valid_api_version_from_parameters(
    const rav::PathMatcher::Parameters& params, const VersionsContainer& versions,
    const std::string_view param_name = "version"
) {
    const auto version_str = params.get(param_name);
    if (version_str == nullptr) {
        return std::nullopt;
    }
    auto version = rav::nmos::ApiVersion::from_string(*version_str);

    if (!version.has_value()) {
        return std::nullopt;
    }

    auto found = std::any_of(versions.begin(), versions.end(), [&version](const auto& supported_version) {
        return supported_version == version;
    });

    if (!found) {
        return std::nullopt;
    }

    return version;
}

boost::json::array get_sender_transport_params_from_sdp(const rav::sdp::SessionDescription& sdp) {
    boost::json::array transport_params;
    for (auto& media : sdp.media_descriptions) {
        rav::nmos::SenderTransportParamsRtp params {};
        params.rtp_enabled = true;
        params.destination_port = media.port;
        params.source_port = media.port;  // TODO: This is probably not correct. I suspect that this should be the port
                                          // of the sending socket, however we don't bind the socket and so there is no
                                          // port. It's unclear how to acquire the correct port number here.
        if (!media.connection_infos.empty()) {
            params.destination_ip = media.connection_infos.front().address;
        } else {
            RAV_WARNING("No connection info available");
        }
        if (!media.source_filters.empty() && !media.source_filters.front().src_list.empty()) {
            params.source_ip = media.source_filters.front().src_list.front();
        } else {
            RAV_WARNING("No source filter available");
        }
        transport_params.push_back(boost::json::value_from(params));
    }
    return transport_params;
}

boost::json::array get_receiver_transport_params_from_sdp(const rav::sdp::SessionDescription& sdp) {
    boost::json::array transport_params;
    for (auto& media : sdp.media_descriptions) {
        rav::nmos::ReceiverTransportParamsRtp params {};
        params.interface_ip = sdp.origin.unicast_address;
        params.destination_port = media.port;
        params.rtp_enabled = true;
        if (!media.connection_infos.empty()) {
            params.multicast_ip = media.connection_infos.front().address;
        } else {
            RAV_WARNING("No connection info available");
        }
        if (!media.source_filters.empty() && !media.source_filters.front().src_list.empty()) {
            params.source_ip = media.source_filters.front().src_list.front();
        } else {
            RAV_WARNING("No source filter available");
        }
        transport_params.push_back(boost::json::value_from(params));
    }
    return transport_params;
}

boost::json::array get_sender_constraints_from_sdp(const rav::sdp::SessionDescription& sdp) {
    boost::json::array constraints_array;
    for (auto& media : sdp.media_descriptions) {
        std::ignore = media;
        rav::nmos::ConstraintsRtp constraints {};
        constraints.destination_ip = rav::nmos::Constraint();
        constraints.source_port = rav::nmos::Constraint();
        constraints_array.push_back(boost::json::value_from(constraints));
    }
    return constraints_array;
}

boost::json::array get_receiver_constraints_from_sdp(const rav::sdp::SessionDescription& sdp) {
    boost::json::array constraints_array;
    for (auto& media : sdp.media_descriptions) {
        std::ignore = media;
        rav::nmos::ConstraintsRtp constraints {};
        constraints.multicast_ip = rav::nmos::Constraint();
        constraints.interface_ip = rav::nmos::Constraint();
        constraints_array.push_back(boost::json::value_from(constraints));
    }
    return constraints_array;
}

}  // namespace

std::array<rav::nmos::ApiVersion, 2> rav::nmos::Node::k_node_api_versions = {{
    ApiVersion {1, 2},
    ApiVersion {1, 3},
}};

std::array<rav::nmos::ApiVersion, 2> rav::nmos::Node::k_connection_api_versions = {{
    ApiVersion {1, 0},
    ApiVersion {1, 1},
}};

boost::system::result<void, rav::nmos::Error> rav::nmos::Node::Configuration::validate() const {
    bool version_valid = false;

    for (auto& v : k_node_api_versions) {
        if (v == api_version) {
            version_valid = true;
            break;
        }
    }

    if (!version_valid) {
        return Error::invalid_api_version;
    }

    if (id.is_nil()) {
        return Error::invalid_id;
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

boost::json::object rav::nmos::Node::Configuration::to_json() const {
    return {
        {"id", to_string(id)},
        {"operation_mode", to_string(operation_mode)},
        {"api_version", api_version.to_string()},
        {"registry_address", registry_address},
        {"enabled", enabled},
        {"api_port", api_port},
        {"label", label},
        {"description", description},
    };
}

boost::system::result<rav::nmos::Node::Configuration, std::string>
rav::nmos::Node::Configuration::from_json(const boost::json::value& json) {
    try {
        Configuration config {};

        // UUID
        const auto& uuid_str = json.at("id").as_string();
        const auto uuid = boost::lexical_cast<boost::uuids::uuid>(std::string_view(uuid_str));
        if (uuid.is_nil()) {
            return {"Invalid UUID"};
        }
        config.id = uuid;

        // Operation mode
        const auto& operation_mode_str = json.at("operation_mode").as_string();
        const auto operation_mode = operation_mode_from_string(operation_mode_str);
        if (!operation_mode.has_value()) {
            return fmt::format("Invalid operation mode: {}", std::string_view(operation_mode_str.c_str()));
        }
        config.operation_mode = *operation_mode;

        // Api version
        const auto& api_version_str = json.at("api_version").as_string();
        const auto api_version = ApiVersion::from_string(api_version_str);
        if (!api_version) {
            return fmt::format("Invalid API version: {}", std::string_view(api_version_str));
        }
        config.api_version = *api_version;

        config.registry_address = json.at("registry_address").as_string();
        config.enabled = json.at("enabled").as_bool();
        config.api_port = json.at("api_port").to_number<uint16_t>();
        config.label = json.at("label").as_string();
        config.description = json.at("description").as_string();
        return config;
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

    for (auto& v : k_node_api_versions) {
        self_.api.versions.push_back(v.to_string());
    }

    ClockInternal clock_int;
    clock_int.name = k_internal_clock_name;
    self_.clocks.emplace_back(clock_int);

    ClockPtp clock_ptp;
    clock_ptp.name = k_ptp_clock_name;
    clock_ptp.gmid = "00-00-00-00-00-00-00-00";
    self_.clocks.emplace_back(clock_ptp);  // Must be at k_clock_ptp_index

    http_server_.get("/", [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        ok_response(res, boost::json::serialize(boost::json::array({"x-nmos/"})));
    });

    http_server_.get("/x-nmos", [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        ok_response(res, boost::json::serialize(boost::json::array({"node/", "connection/"})));
    });

    // MARK: Node API

    http_server_.get(
        "/x-nmos/node",
        [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
            res.result(http::status::ok);
            set_default_headers(res);

            boost::json::array versions;
            for (const auto& version : k_node_api_versions) {
                versions.push_back({fmt::format("{}/", version.to_string())});
            }

            res.body() = boost::json::serialize(versions);
            res.prepare_payload();
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
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
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            ok_response(res, boost::json::serialize(boost::json::value_from(self_)));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/devices",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            boost::json::array devices;

            for (const auto* device : devices_) {
                devices.push_back(boost::json::value_from(*device));
            }

            ok_response(res, boost::json::serialize(devices));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/devices/{device_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
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
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            boost::json::array flows;

            for (auto* flow : flows_) {
                flows.push_back(boost::json::value_from(*flow));
            }

            ok_response(res, boost::json::serialize(flows));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/flows/{flow_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
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
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            boost::json::array receivers;

            for (auto* receiver : receivers_) {
                receivers.push_back(boost::json::value_from(*receiver));
            }

            ok_response(res, boost::json::serialize(receivers));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/receivers/{receiver_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
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
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
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
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            boost::json::array senders;

            for (auto* sender : senders_) {
                senders.push_back(boost::json::value_from(*sender));
            }

            ok_response(res, boost::json::serialize(senders));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/senders/{sender_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
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
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            boost::json::array sources;

            for (auto* source : sources_) {
                sources.push_back(boost::json::value_from(*source));
            }

            ok_response(res, boost::json::serialize(sources));
        }
    );

    http_server_.get(
        "/x-nmos/node/{version}/sources/{source_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_node_api_versions).has_value()) {
                return invalid_api_version_response(res);
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

    // MARK: Connection API

    http_server_.get(
        "/x-nmos/connection",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters&) {
            res.result(http::status::ok);
            set_default_headers(res);

            boost::json::array versions;
            for (const auto& version : k_connection_api_versions) {
                versions.push_back({fmt::format("{}/", version.to_string())});
            }

            res.body() = boost::json::serialize(versions);
            res.prepare_payload();
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            ok_response(res, boost::json::serialize(boost::json::array({"bulk/", "single/"})));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/bulk",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            ok_response(res, boost::json::serialize(boost::json::array({"senders/", "receivers/"})));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            ok_response(res, boost::json::serialize(boost::json::array({"senders/", "receivers/"})));
        }
    );

    // MARK: Connection API - Receivers

    http_server_.get(
        "/x-nmos/connection/{version}/bulk/receivers",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            res.base().result(403);
            res.base().reason("Forbidden");
        }
    );

    http_server_.options(
        "/x-nmos/connection/{version}/bulk/receivers",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            ok_response(res, {});
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/receivers",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            auto array = boost::json::array();

            for (auto& receiver : receivers_) {
                array.push_back(boost::json::value_from(fmt::format("{}/", boost::uuids::to_string(receiver->id))));
            }

            ok_response(res, boost::json::serialize(array));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/receivers/{receiver_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* receiver_id = params.get("receiver_id");
            if (receiver_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "No receiver ID provided");
                return;
            }

            auto* receiver = find_receiver(boost::uuids::string_generator()(*receiver_id));
            if (receiver == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Receiver not found");
                return;
            }

            ok_response(
                res,
                boost::json::serialize(boost::json::array({"constraints/", "staged/", "active/", "transporttype/"}))
            );
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/receivers/{receiver_id}/staged",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* receiver_id = params.get("receiver_id");
            if (receiver_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "No receiver ID provided");
                return;
            }

            auto* receiver = find_receiver(boost::uuids::string_generator()(*receiver_id));
            if (receiver == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Receiver not found");
                return;
            }

            boost::json::array transport_params;
            TransportFile transport_file;
            transport_file.type = "application/sdp";
            const auto it = receiver_transport_files_.find(boost::uuids::string_generator()(*receiver_id));
            if (it != receiver_transport_files_.end()) {
                transport_params = get_receiver_transport_params_from_sdp(it->second);
                if (auto transport_file_data = to_string(it->second)) {
                    transport_file.data = *transport_file_data;
                } else {
                    RAV_ERROR("Failed to encode SDP");
                }
            }

            ActivationResponse activation_response;

            const boost::json::value value {
                {"sender_id", json_value_from_uuid(receiver->subscription.sender_id)},
                {"master_enable", receiver->subscription.active},
                {"activation", boost::json::value_from(activation_response)},
                {"transport_params", transport_params},
                {"transport_file", boost::json::value_from(transport_file)},
            };

            ok_response(res, boost::json::serialize(value));
        }
    );

    http_server_.options(
        "/x-nmos/connection/{version}/single/receivers/{receiver_id}/staged",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* receiver_id = params.get("receiver_id");
            if (receiver_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "No receiver ID provided");
                return;
            }

            auto* receiver = find_receiver(boost::uuids::string_generator()(*receiver_id));
            if (receiver == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Receiver not found");
                return;
            }

            ok_response(res, {});
        }
    );

    http_server_.patch(
        "/x-nmos/connection/{version}/single/receivers/{receiver_id}/staged",
        [this](const HttpServer::Request& request, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* receiver_id = params.get("receiver_id");
            if (receiver_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "No receiver ID provided");
                return;
            }

            auto* receiver = find_receiver(boost::uuids::string_generator()(*receiver_id));
            if (receiver == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Receiver not found");
                return;
            }

            auto body = request.body();
            RAV_TRACE("{} {}: {}", request.method_string(), std::string(request.target()), body);

            auto json = boost::json::parse(body);

            if (!json.is_object()) {
                set_error_response(res, http::status::bad_request, "Bad Request", "Expected a JSON object");
                return;
            }

            // Validate object
            for (auto& member : json.as_object()) {
                if (member.key() == "activation") {
                    continue;
                }
                if (member.key() == "sender_id") {
                    continue;
                }
                if (member.key() == "transport_params") {
                    continue;
                }
                if (member.key() == "transport_file") {
                    continue;
                }
                if (member.key() == "master_enable") {
                    continue;
                }
                set_error_response(
                    res, http::status::bad_request, "Bad Request",
                    "Invalid JSON: unexpected key: " + std::string(member.key())
                );
                return;
            }

            // Refuse activation modes other than activate_immediate
            if (const auto result = json.try_at("activation")) {
                auto activation = boost::json::value_to<Activation>(*result);
                if (const auto mode = activation.mode) {
                    if (*mode != Activation::Mode::activate_immediate) {
                        set_error_response(
                            res, http::status::not_implemented, "Not Implemented",
                            "Only activate_immediate is implemented"
                        );
                        return;
                    }
                }
            }

            if (auto result = receiver->on_patch_request(json); !result) {
                set_error_response(res, http::status::bad_request, "Bad Request", result.error());
                return;
            }

            boost::json::array transport_params;
            TransportFile transport_file;
            transport_file.type = "application/sdp";

            const auto it = receiver_transport_files_.find(boost::uuids::string_generator()(*receiver_id));
            if (it != receiver_transport_files_.end()) {
                transport_params = get_receiver_transport_params_from_sdp(it->second);
                if (auto transport_file_data = to_string(it->second)) {
                    transport_file.data = *transport_file_data;
                } else {
                    RAV_ERROR("Failed to encode SDP");
                }
            }

            ActivationResponse activation_response;

            const boost::json::value value {
                {"sender_id", json_value_from_uuid(receiver->subscription.sender_id)},
                {"master_enable", receiver->subscription.active},
                {"activation", boost::json::value_from(activation_response)},
                {"transport_params", transport_params},
                {"transport_file", boost::json::value_from(transport_file)},
            };

            ok_response(res, boost::json::serialize(value));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/receivers/{receiver_id}/active",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* receiver_id = params.get("receiver_id");
            if (receiver_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "No receiver ID provided");
                return;
            }

            auto* receiver = find_receiver(boost::uuids::string_generator()(*receiver_id));
            if (receiver == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Receiver not found");
                return;
            }

            boost::json::array transport_params;
            TransportFile transport_file;
            transport_file.type = "application/sdp";

            const auto it = receiver_transport_files_.find(boost::uuids::string_generator()(*receiver_id));
            if (it != receiver_transport_files_.end()) {
                transport_params = get_receiver_transport_params_from_sdp(it->second);
                if (auto transport_file_data = to_string(it->second)) {
                    transport_file.data = *transport_file_data;
                } else {
                    RAV_ERROR("Failed to encode SDP");
                }
            }

            ActivationResponse activation_response;

            const boost::json::value value {
                {"sender_id", json_value_from_uuid(receiver->subscription.sender_id)},
                {"master_enable", receiver->subscription.active},
                {"activation", boost::json::value_from(activation_response)},
                {"transport_params", transport_params},
                {"transport_file", boost::json::value_from(transport_file)},
            };

            ok_response(res, boost::json::serialize(value));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/receivers/{receiver_id}/constraints",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* receiver_id = params.get("receiver_id");
            if (receiver_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "No receiver ID provided");
                return;
            }

            auto* receiver = find_receiver(boost::uuids::string_generator()(*receiver_id));
            if (receiver == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Receiver not found");
                return;
            }

            boost::json::array constraints;
            const auto transport_file = receiver_transport_files_.find(boost::uuids::string_generator()(*receiver_id));
            if (transport_file != receiver_transport_files_.end()) {
                constraints = get_receiver_constraints_from_sdp(transport_file->second);
            }

            ok_response(res, boost::json::serialize(constraints));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/receivers/{receiver_id}/transporttype",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* receiver_id = params.get("receiver_id");
            if (receiver_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid receiver ID", "No receiver ID provided");
                return;
            }

            auto* receiver = find_receiver(boost::uuids::string_generator()(*receiver_id));
            if (receiver == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Receiver not found");
                return;
            }

            ok_response(res, boost::json::serialize("urn:x-nmos:transport:rtp"));
        }
    );

    // MARK: Connection API - Senders

    http_server_.get(
        "/x-nmos/connection/{version}/bulk/senders",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            set_error_response(res, http::status::forbidden, "Forbidden", {});
        }
    );

    http_server_.options(
        "/x-nmos/connection/{version}/bulk/senders",
        [](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            ok_response(res, {});
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/senders",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            auto array = boost::json::array();

            for (auto& sender : senders_) {
                array.push_back(boost::json::value_from(fmt::format("{}/", boost::uuids::to_string(sender->id))));
            }

            ok_response(res, boost::json::serialize(array));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/senders/{sender_id}",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* sender_id = params.get("sender_id");
            if (sender_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "No sender ID provided");
                return;
            }

            auto* sender = find_sender(boost::uuids::string_generator()(*sender_id));
            if (sender == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Sender not found");
                return;
            }

            ok_response(
                res,
                boost::json::serialize(
                    boost::json::array({"constraints/", "staged/", "active/", "transportfile/", "transporttype/"})
                )
            );
        }
    );

    http_server_.options(
        "/x-nmos/connection/{version}/single/senders/{sender_id}/staged",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* sender_id = params.get("sender_id");
            if (sender_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "No sender ID provided");
                return;
            }

            auto* sender = find_sender(boost::uuids::string_generator()(*sender_id));
            if (sender == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Sender not found");
                return;
            }

            ok_response(res, {});
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/senders/{sender_id}/staged",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* sender_id = params.get("sender_id");
            if (sender_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "No sender ID provided");
                return;
            }

            auto* sender = find_sender(boost::uuids::string_generator()(*sender_id));
            if (sender == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Sender not found");
                return;
            }

            const auto transport_file = sender_transport_files_.find(sender);
            if (transport_file == sender_transport_files_.end()) {
                set_error_response(res, http::status::not_found, "Not found", "Sender transport file not found");
                return;
            }

            ActivationResponse activation_response;
            auto transport_params = get_sender_transport_params_from_sdp(transport_file->second);

            const boost::json::value value {
                {"receiver_id", boost::json::value_from(sender->subscription.receiver_id)},
                {"master_enable", sender->subscription.active},
                {"activation", boost::json::value_from(activation_response)},
                {"transport_params", transport_params},
            };

            ok_response(res, boost::json::serialize(value));
        }
    );

    http_server_.patch(
        "/x-nmos/connection/{version}/single/senders/{sender_id}/staged",
        [this](const HttpServer::Request& req, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* sender_id = params.get("sender_id");
            if (sender_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "No sender ID provided");
                return;
            }

            auto* sender = find_sender(boost::uuids::string_generator()(*sender_id));
            if (sender == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Sender not found");
                return;
            }

            auto body = req.body();
            RAV_TRACE("{} {}: {}", req.method_string(), std::string(req.target()), body);

            auto json = boost::json::parse(body);

            // Validate object
            for (auto& member : json.as_object()) {
                if (member.key() == "activation") {
                    continue;
                }
                if (member.key() == "receiver_id") {
                    continue;
                }
                if (member.key() == "transport_params") {
                    continue;
                }
                if (member.key() == "master_enable") {
                    continue;
                }
                set_error_response(
                    res, http::status::bad_request, "Bad Request",
                    "Invalid JSON: unexpected key: " + std::string(member.key())
                );
                return;
            }

            if (auto result = json.try_at("receiver_id")) {
                auto new_receiver_id = uuid_from_json(*result);
                if (!sender->patch_receiver_id(new_receiver_id)) {
                    set_error_response(res, http::status::bad_request, "Bad Request", "Failed to change receiver id");
                    return;
                }
            }

            if (auto result = json.try_at("transport_params")) {
                if (!result->is_array()) {
                    set_error_response(
                        res, http::status::bad_request, "Bad Request", "Transport params should be an array"
                    );
                    return;
                }

                for (auto& p : result->as_array()) {
                    auto transport_params = boost::json::value_to<SenderTransportParamsRtp>(p);
                    if (transport_params.source_ip.has_value() && transport_params.source_ip != "auto") {
                        set_error_response(
                            res, http::status::bad_request, "Bad Request", "Changing source ip is not allowed"
                        );
                        return;
                    }

                    if (!std::holds_alternative<std::monostate>(transport_params.source_port)) {
                        auto* source_port = std::get_if<std::string>(&transport_params.source_port);
                        if (source_port == nullptr || *source_port != "auto") {
                            set_error_response(
                                res, http::status::not_implemented, "Not Implemented",
                                "Changing source port is not implemented"
                            );
                            return;
                        }
                    }

                    if (transport_params.destination_ip.has_value() && transport_params.destination_ip != "auto") {
                        set_error_response(
                            res, http::status::bad_request, "Bad Request", "Changing destination ip is not allowed"
                        );
                        return;
                    }

                    if (!std::holds_alternative<std::monostate>(transport_params.destination_port)) {
                        auto* destination_port = std::get_if<std::string>(&transport_params.destination_port);
                        if (destination_port == nullptr || *destination_port != "auto") {
                            set_error_response(
                                res, http::status::not_implemented, "Not Implemented",
                                "Changing destination port is not implemented"
                            );
                            return;
                        }
                    }

                    if (transport_params.rtp_enabled.has_value()) {
                        set_error_response(
                            res, http::status::bad_request, "Bad Request", "Changing RTP enabled is not allowed"
                        );
                        return;
                    }
                }
            }

            auto activation = json.try_at("activation");
            auto transport = json.try_at("transport_params");

            if (activation && !transport) {
                set_error_response(
                    res, http::status::bad_request, "Bad Request", "Invalid JSON: expecting transport_params"
                );
                return;
            }

            const auto transport_file = sender_transport_files_.find(sender);
            if (transport_file == sender_transport_files_.end()) {
                set_error_response(res, http::status::not_found, "Not Found", "Sender transport file not found");
                return;
            }

            ActivationResponse activation_response;
            auto transport_params = get_sender_transport_params_from_sdp(transport_file->second);

            const boost::json::value value {
                {"receiver_id", json_value_from_uuid(sender->subscription.receiver_id)},
                {"master_enable", sender->subscription.active},
                {"activation", boost::json::value_from(activation_response)},
                {"transport_params", transport_params},
            };

            ok_response(res, boost::json::serialize(value));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/senders/{sender_id}/active",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* sender_id = params.get("sender_id");
            if (sender_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "No sender ID provided");
                return;
            }

            auto* sender = find_sender(boost::uuids::string_generator()(*sender_id));
            if (sender == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Sender not found");
                return;
            }

            const auto transport_file = sender_transport_files_.find(sender);
            if (transport_file == sender_transport_files_.end()) {
                set_error_response(res, http::status::not_found, "Not found", "Sender transport file not found");
                return;
            }

            ActivationResponse activation_response;
            auto transport_params = get_sender_transport_params_from_sdp(transport_file->second);

            const boost::json::value value {
                {"receiver_id", json_value_from_uuid(sender->subscription.receiver_id)},
                {"master_enable", sender->subscription.active},
                {"activation", boost::json::value_from(activation_response)},
                {"transport_params", transport_params},
            };

            ok_response(res, boost::json::serialize(value));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/senders/{sender_id}/constraints",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* sender_id = params.get("sender_id");
            if (sender_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "No sender ID provided");
                return;
            }

            auto* sender = find_sender(boost::uuids::string_generator()(*sender_id));
            if (sender == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Sender not found");
                return;
            }

            boost::json::array constraints;
            const auto transport_file = sender_transport_files_.find(sender);
            if (transport_file != sender_transport_files_.end()) {
                constraints = get_sender_constraints_from_sdp(transport_file->second);
            }

            ok_response(res, boost::json::serialize(constraints));
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/senders/{sender_id}/transportfile",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* sender_id = params.get("sender_id");
            if (sender_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "No sender ID provided");
                return;
            }

            auto* sender = find_sender(boost::uuids::string_generator()(*sender_id));
            if (sender == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Sender not found");
                return;
            }

            const auto it = sender_transport_files_.find(sender);
            if (it == sender_transport_files_.end()) {
                set_error_response(res, http::status::not_found, "Not found", "Sender transport file not found");
                return;
            }
            const auto sdp_text = sdp::to_string(it->second);
            if (!sdp_text) {
                set_error_response(res, http::status::no_content, "No content", "Failed to generate SDP text");
            }
            ok_response(res, *sdp_text, "application/sdp");
        }
    );

    http_server_.get(
        "/x-nmos/connection/{version}/single/senders/{sender_id}/transporttype",
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
            if (!get_valid_api_version_from_parameters(params, k_connection_api_versions).has_value()) {
                return invalid_api_version_response(res);
            }

            const auto* sender_id = params.get("sender_id");
            if (sender_id == nullptr) {
                set_error_response(res, http::status::bad_request, "Invalid sender ID", "No sender ID provided");
                return;
            }

            auto* sender = find_sender(boost::uuids::string_generator()(*sender_id));
            if (sender == nullptr) {
                set_error_response(res, http::status::not_found, "Not found", "Sender not found");
                return;
            }

            ok_response(res, boost::json::serialize("urn:x-nmos:transport:rtp"));
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
    if (status_ == Status::registered) {
        unregister_async();
    }
    set_status(Status::disabled);
}

tl::expected<void, rav::nmos::Error>
rav::nmos::Node::set_configuration(Configuration new_configuration, const bool force_update) {
    if (new_configuration == configuration_ && !force_update) {
        return {};  // Nothing changed, so we should be in the correct state.
    }

    if (new_configuration.enabled) {
        const auto result = new_configuration.validate();
        if (result.has_error()) {
            return tl::unexpected(result.error());
        }
    }

    bool unregister = false;

    if (status_ == Status::registered) {
        if (configuration_.api_version != new_configuration.api_version) {
            unregister = true;
        }

        if (configuration_.id != new_configuration.id) {
            unregister = true;
        }

        if (!new_configuration.enabled) {
            unregister = true;
        }
    }

    if (unregister) {
        unregister_async();  // Before configuration_ is overwritten
    }

    bool restart = false;

    if (configuration_.enabled != new_configuration.enabled) {
        restart = true;
    }

    if (configuration_.operation_mode != new_configuration.operation_mode) {
        restart = true;
    }

    if (configuration_.api_port != new_configuration.api_port) {
        restart = true;
    }

    configuration_ = std::move(new_configuration);
    update_self();

    if (restart) {
        stop_internal();
    }

    if (restart && configuration_.enabled) {
        const auto result = start_internal();
        if (result.has_error()) {
            set_status(Status::error);
            return tl::unexpected(result.error());
        }
    }

    if (!configuration_.enabled) {
        set_status(Status::disabled);
    }

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }
    on_configuration_changed(configuration_);

    return {};
}

const rav::nmos::Node::Configuration& rav::nmos::Node::get_configuration() const {
    return configuration_;
}

boost::system::result<void, rav::nmos::Error> rav::nmos::Node::start_internal() {
    const auto result = http_server_.start("0.0.0.0", configuration_.api_port);
    if (result.has_error()) {
        RAV_ERROR("Failed to start HTTP server: {}", result.error().message());
        return Error::failed_to_start_http_server;
    }

    const auto http_endpoint = http_server_.get_local_endpoint();
    for (auto& endpoint : self_.api.endpoints) {
        endpoint.port = http_endpoint.port();
    }

    for (auto& i : network_interface_config_.get_array_of_interface_addresses<2>()) {
        if (!i.is_unspecified()) {
            self_.href = fmt::format("http://{}:{}", i.to_string(), http_endpoint.port());
            break;
        }
    }

    status_info_.api_port = http_endpoint.port();

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
        status_info_.name = "(custom registry)";
        const auto port = url->port().empty() ? (url->scheme() == "https" ? "443" : "80") : url->port();
        connect_to_registry_async(url->host(), port);
        return {};
    }

    if (configuration_.operation_mode == OperationMode::p2p) {
        selected_registry_.reset();
        registry_browser_->stop();
        set_status(Status::p2p);
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
            set_status(Status::discovering);
        } else {
            set_status(Status::p2p);
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
    status_info_ = {};
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
            set_status(Status::error);
            return;
        }

        if (result.value().result() != http::status::ok) {
            RAV_ERROR("Unexpected response from NMOS registry: {}", result.value().result_int());
            set_status(Status::error);
            return;
        }

        if (post_resource_error_count_ > 0) {
            RAV_ERROR("Failed to post one or more resources to the NMOS registry");
            set_status(Status::error);
            return;
        }

        RAV_INFO("Registered with NMOS registry");
        set_status(Status::registered);

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
                set_status(Status::error);
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
                set_status(Status::error);
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
                set_status(Status::error);
                // When this case happens, it's pretty reasonable to assume that the connection is lost.
            } else {
                RAV_ERROR("Sending heartbeat failed: {}", result->result_int());
                if (failed_heartbeat_count_ < k_max_failed_heartbeats) {
                    return;  // Don't try to reconnect yet, just try the next heartbeat.
                }
            }
            http_client_->cancel_outstanding_requests();
            RAV_ERROR("Failed to send heartbeat {} times, stopping heartbeat", failed_heartbeat_count_);
            set_status(Status::error);
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
            set_status(Status::error);
            timer_.once(k_default_timeout, [this] {
                connect_to_registry_async();  // Retry connection
            });
        } else if (result.value().result() != http::status::ok) {
            RAV_ERROR("Unexpected response from NMOS registry: {}", result.value().result_int());
            set_status(Status::error);
        } else {
            register_async();
            set_status(Status::connected);
        }
    });
}

void rav::nmos::Node::connect_to_registry_async(const std::string_view host, const std::string_view service) {
    http_client_->set_host(host, service);
    status_info_.address = fmt::format("http://{}:{}", host, service);
    set_status(Status::connecting);
    connect_to_registry_async();
}

bool rav::nmos::Node::add_receiver_to_device(const ReceiverAudio& receiver) const {
    for (auto* device : devices_) {
        if (device->id == receiver.device_id) {
            device->receivers.push_back(receiver.id);
            return true;
        }
    }
    return false;
}

bool rav::nmos::Node::add_sender_to_device(const Sender& sender) const {
    for (auto* device : devices_) {
        if (device->id == sender.device_id) {
            device->senders.push_back(sender.id);
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
    status_info_.name = desc.name;
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

void rav::nmos::Node::set_status(const Status new_status) {
    if (status_ == new_status) {
        return;  // No change in status, nothing to do.
    }
    status_ = new_status;
    if (status_ == Status::disabled) {
        selected_registry_.reset();
    }
    on_status_changed(status_, status_info_);
}

void rav::nmos::Node::update_all_resources_to_now() {
    const auto version = Version(get_local_clock().now());

    self_.version = version;

    for (auto* device : devices_) {
        device->version = version;
    }

    for (auto* source : sources_) {
        source->version = version;
    }

    for (auto* flow : flows_) {
        flow->version = version;
    }

    for (auto* sender : senders_) {
        sender->version = version;
    }

    for (auto* receiver : receivers_) {
        receiver->version = version;
    }
}

void rav::nmos::Node::send_updated_resources_async() {
    auto new_version = Version(get_local_clock().now());

    if (self_.version > current_version_) {
        post_resource_async("node", boost::json::value_from(self_));
    }

    for (auto* device : devices_) {
        if (device->version > current_version_) {
            post_resource_async("device", boost::json::value_from(*device));
        }
    }

    for (auto* source : sources_) {
        if (source->version > current_version_) {
            post_resource_async("source", boost::json::value_from(*source));
        }
    }

    for (auto* flow : flows_) {
        if (flow->version > current_version_) {
            post_resource_async("flow", boost::json::value_from(*flow));
        }
    }

    for (auto* sender : senders_) {
        if (sender->version > current_version_) {
            post_resource_async("sender", boost::json::value_from(*sender));
        }
    }

    for (auto* receiver : receivers_) {
        if (receiver->version > current_version_) {
            post_resource_async("receiver", boost::json::value_from(*receiver));
        }
    }

    current_version_ = Version(new_version);
}

void rav::nmos::Node::update_device(Device& device) {
    const auto endpoint = http_server_.get_local_endpoint();
    device.controls.clear();
    for (auto& ip : network_interface_config_.get_interface_ipv4_addresses()) {
        Device::Control control;
        control.type = "urn:x-nmos:control:sr-ctrl/v1.1";
        control.href = fmt::format("http://{}:{}/x-nmos/connection/v1.1", ip.to_string(), endpoint.port());
        device.controls.push_back(control);
    }
    device.version = Version(get_local_clock().now());
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

bool rav::nmos::Node::add_or_update_device(Device* device) {
    RAV_ASSERT(device != nullptr, "Device should not be nullptr");
    RAV_ASSERT(!device->id.is_nil(), "Device ID should not be nil");
    RAV_ASSERT(!self_.id.is_nil(), "Node ID should not be nil");

    device->node_id = self_.id;
    device->version.update(get_local_clock().now());
    update_device(*device);

    // Test is a device with the same uuid exists
    for (const auto* existing_device : devices_) {
        if (existing_device != device && existing_device->id == device->id) {
            RAV_ERROR("Device with same uuid already exists");
            return false;
        }
    }

    bool device_exists = false;
    for (const auto* existing_device : devices_) {
        if (existing_device == device) {
            device_exists = true;
        }
    }

    if (!device_exists) {
        devices_.push_back(device);
    }

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }

    return true;
}

const rav::nmos::Device* rav::nmos::Node::find_device(const boost::uuids::uuid& uuid) const {
    const auto it = std::find_if(devices_.begin(), devices_.end(), [uuid](const Device* device) {
        return device->id == uuid;
    });
    if (it != devices_.end()) {
        return *it;
    }
    return nullptr;
}

bool rav::nmos::Node::add_or_update_flow(FlowAudioRaw* flow) {
    RAV_ASSERT(flow != nullptr, "Flow should not be nullptr");

    if (flow->id.is_nil()) {
        RAV_ERROR("Flow ID should not be nil");
        return false;
    }

    flow->version = Version(get_local_clock().now());

    const auto it = std::find_if(flows_.begin(), flows_.end(), [flow](const FlowAudioRaw* existing_flow) {
        return existing_flow == flow;
    });

    if (it == flows_.end()) {
        flows_.push_back(flow);
    }

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }

    return true;
}

bool rav::nmos::Node::remove_device(Device* device) {
    RAV_ASSERT(device != nullptr, "Device is null");

    stl_remove_if(senders_, [&device](const Sender* s) {
        return s->device_id == device->id;
    });

    stl_remove_if(flows_, [&device](const FlowAudioRaw* f) {
        return f->device_id == device->id;
    });

    stl_remove_if(sources_, [&device](const SourceAudio* s) {
        return s->device_id == device->id;
    });

    stl_remove_if(receivers_, [&device](const ReceiverAudio* r) {
        return r->device_id == device->id;
    });

    const auto count = stl_remove_if(devices_, [device](const Device* d) {
        return d == device;
    });

    if (status_ == Status::registered && count > 0) {
        delete_resource_async("devices", device->id);
    }

    return count > 0;
}

const rav::nmos::FlowAudioRaw* rav::nmos::Node::find_flow(const boost::uuids::uuid& uuid) const {
    const auto it = std::find_if(flows_.begin(), flows_.end(), [uuid](const FlowAudioRaw* flow) {
        return flow->id == uuid;
    });
    if (it != flows_.end()) {
        return *it;
    }
    return nullptr;
}

bool rav::nmos::Node::remove_flow(FlowAudioRaw* flow) {
    const auto count = stl_remove_if(flows_, [&flow](const FlowAudioRaw* f) {
        return f == flow;
    });

    if (status_ == Status::registered && count > 0) {
        delete_resource_async("flows", flow->id);
    }

    return count > 0;
}

bool rav::nmos::Node::add_or_update_receiver(ReceiverAudio* receiver) {
    RAV_ASSERT(receiver != nullptr, "Expecting receiver to be a valid pointer");

    if (receiver->id.is_nil()) {
        RAV_ERROR("Receiver ID should not be nil");
        return false;
    }

    receiver->version = Version(get_local_clock().now());

    const auto it =
        std::find_if(receivers_.begin(), receivers_.end(), [receiver](const ReceiverAudio* existing_receiver) {
            return receiver == existing_receiver;
        });

    if (it == receivers_.end()) {
        if (!add_receiver_to_device(*receiver)) {
            RAV_ERROR("Device not found");
            return false;
        }
        receivers_.push_back(receiver);
    }

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }

    return true;
}

const rav::nmos::ReceiverAudio* rav::nmos::Node::find_receiver(const boost::uuids::uuid& uuid) const {
    const auto it = std::find_if(receivers_.begin(), receivers_.end(), [uuid](const ReceiverAudio* receiver) {
        return receiver->id == uuid;
    });
    if (it != receivers_.end()) {
        return *it;
    }
    return nullptr;
}

bool rav::nmos::Node::remove_receiver(ReceiverAudio* receiver) {
    RAV_ASSERT(receiver != nullptr, "Expecting receiver to be a valid pointer");

    const auto count = stl_remove_if(receivers_, [&receiver](const ReceiverAudio* d) {
        return d == receiver;
    });

    if (status_ == Status::registered && count > 0) {
        delete_resource_async("receivers", receiver->id);
    }

    return count > 0;
}

bool rav::nmos::Node::add_or_update_sender(Sender* sender) {
    if (sender->id.is_nil()) {
        RAV_ERROR("Sender ID should not be nil");
        return false;
    }

    sender->version = Version(get_local_clock().now());

    const auto it = std::find_if(senders_.begin(), senders_.end(), [sender](const Sender* s) {
        return s->id == sender->id;
    });

    if (it == senders_.end()) {
        if (!add_sender_to_device(*sender)) {
            RAV_ERROR("Device not found");
            return false;
        }
        senders_.push_back(sender);
    }

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }

    return true;
}

void rav::nmos::Node::set_sender_transport_file(Sender* sender, std::optional<sdp::SessionDescription> transport_file) {
    RAV_ASSERT(sender != nullptr, "Expected sender to be a valid pointer");
    RAV_ASSERT(!sender->id.is_nil(), "Sender uuid should be valid");
    if (!transport_file.has_value()) {
        sender_transport_files_.erase(sender);
        return;
    }
    sender_transport_files_[sender] = std::move(*transport_file);
}

void rav::nmos::Node::set_receiver_transport_file(
    const boost::uuids::uuid receiver_uuid, std::optional<sdp::SessionDescription> transport_file
) {
    RAV_ASSERT(!receiver_uuid.is_nil(), "Sender uuid should be valid");
    if (!transport_file.has_value()) {
        receiver_transport_files_.erase(receiver_uuid);
        return;
    }
    receiver_transport_files_[receiver_uuid] = std::move(*transport_file);
}

rav::nmos::Sender* rav::nmos::Node::find_sender(const boost::uuids::uuid& uuid) const {
    const auto it = std::find_if(senders_.begin(), senders_.end(), [uuid](const Sender* sender) {
        return sender->id == uuid;
    });
    if (it != senders_.end()) {
        return *it;
    }
    return nullptr;
}

bool rav::nmos::Node::remove_sender(Sender* sender) {
    const auto count = stl_remove_if(senders_, [&sender](const Sender* s) {
        return s == sender;
    });

    if (status_ == Status::registered && count > 0) {
        delete_resource_async("senders", sender->id);
    }

    return count > 0;
}

bool rav::nmos::Node::add_or_update_source(SourceAudio* source) {
    RAV_ASSERT(source != nullptr, "Source must not be null");

    if (source->id.is_nil()) {
        RAV_ERROR("Source ID should not be nil");
        return false;
    }

    source->version = Version(get_local_clock().now());

    const auto it = std::find_if(sources_.begin(), sources_.end(), [&source](const SourceAudio* existing_source) {
        return existing_source->id == source->id;
    });

    if (it == sources_.end()) {
        sources_.push_back(std::move(source));
    }

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }

    return true;
}

const rav::nmos::SourceAudio* rav::nmos::Node::find_source(const boost::uuids::uuid& uuid) const {
    const auto it = std::find_if(sources_.begin(), sources_.end(), [uuid](const SourceAudio* source) {
        return source->id == uuid;
    });
    if (it != sources_.end()) {
        return *it;
    }
    return nullptr;
}

bool rav::nmos::Node::remove_source(SourceAudio* source) {
    const auto count = stl_remove_if(sources_, [&source](const SourceAudio* s) {
        return s == source;
    });

    if (status_ == Status::registered && count > 0) {
        delete_resource_async("sources", source->id);
    }

    return count > 0;
}

const boost::uuids::uuid& rav::nmos::Node::get_uuid() const {
    return self_.id;
}

const std::vector<rav::nmos::Device*>& rav::nmos::Node::get_devices() const {
    return devices_;
}

const std::vector<rav::nmos::FlowAudioRaw*>& rav::nmos::Node::get_flows() const {
    return flows_;
}

const std::vector<rav::nmos::ReceiverAudio*>& rav::nmos::Node::get_receivers() const {
    return receivers_;
}

const std::vector<rav::nmos::Sender*>& rav::nmos::Node::get_senders() const {
    return senders_;
}

const std::vector<rav::nmos::SourceAudio*>& rav::nmos::Node::get_sources() const {
    return sources_;
}

const rav::nmos::Node::Status& rav::nmos::Node::get_status() const {
    return status_;
}

const rav::nmos::Node::StatusInfo& rav::nmos::Node::get_registry_info() const {
    return status_info_;
}

void rav::nmos::Node::set_network_interface_config(NetworkInterfaceConfig config) {
    if (network_interface_config_ == config) {
        return;  // No change in configuration, nothing to do.
    }

    self_.interfaces.clear();
    const auto& system_interfaces = NetworkInterfaceList::get_system_interfaces();

    for (const auto& id : config.interfaces) {
        auto* iface = system_interfaces.get_interface(id);
        if (iface == nullptr) {
            RAV_ERROR("Network interface with ID {} not found", id);
            continue;
        }

        const auto mac_address = iface->get_mac_address();
        if (!mac_address.has_value()) {
            RAV_ERROR("Network interface with ID {} does not have a MAC address", id);
            continue;
        }

        self_.interfaces.emplace_back(Self::Interface {std::nullopt, iface->get_mac_address()->to_string("-"), id});
    }

    auto addrs = config.get_interface_ipv4_addresses();
    if (addrs.empty()) {
        RAV_ERROR("No IPv4 addresses found for the interface");
        return;
    }

    const auto http_endpoint = http_server_.get_local_endpoint();

    self_.api.endpoints.clear();

    for (const auto& ip : addrs) {
        self_.api.endpoints.emplace_back(Self::Endpoint {ip.to_string(), http_endpoint.port(), "http", false});
    }

    for (auto* device : devices_) {
        update_device(*device);
    }

    self_.version.update(get_local_clock().now());

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }

    network_interface_config_ = std::move(config);
}

std::optional<size_t> rav::nmos::Node::index_of_supported_api_version(const ApiVersion& version) {
    const auto it = std::find(k_node_api_versions.begin(), k_node_api_versions.end(), version);
    if (it != k_node_api_versions.end()) {
        return std::distance(k_node_api_versions.begin(), it);
    }
    return std::nullopt;
}

void rav::nmos::Node::ptp_parent_changed(const ptp::ParentDs& parent) {
    if (self_.clocks.size() <= k_clock_ptp_index) {
        RAV_ERROR("PTP clock index out of bounds: {}", k_clock_ptp_index);
        return;
    }

    auto* clock_ptp = std::get_if<ClockPtp>(&self_.clocks[k_clock_ptp_index]);
    if (clock_ptp == nullptr) {
        RAV_ERROR("PTP clock is not of type ClockPtp");
        return;
    }
    clock_ptp->gmid = parent.grandmaster_identity.to_string();
    clock_ptp->traceable = ptp_instance_.get_time_properties_ds().time_traceable;
    self_.version.update(get_local_clock().now());

    if (status_ == Status::registered) {
        send_updated_resources_async();
    }
}

void rav::nmos::Node::ptp_port_changed_state(const ptp::Port&) {
    if (self_.clocks.size() <= k_clock_ptp_index) {
        RAV_ERROR("PTP clock index out of bounds: {}", k_clock_ptp_index);
        return;
    }

    const auto locked = get_local_clock().is_locked();

    auto* clock_ptp = std::get_if<ClockPtp>(&self_.clocks[k_clock_ptp_index]);

    if (clock_ptp == nullptr) {
        RAV_ERROR("PTP clock is not of type ClockPtp");
        return;
    }

    if (clock_ptp->locked == locked) {
        return;
    }

    clock_ptp->locked = locked;
    self_.version.update(get_local_clock().now());
    if (status_ == Status::registered) {
        send_updated_resources_async();
    }
}

void rav::nmos::tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const Node::Configuration& config
) {
    jv = {
        {"id", to_string(config.id)},
        {"operation_mode", to_string(config.operation_mode)},
        {"api_version", config.api_version.to_string()},
        {"registry_address", config.registry_address},
        {"enabled", config.enabled},
        {"api_port", config.api_port},
        {"label", config.label},
        {"description", config.description},
    };
}
