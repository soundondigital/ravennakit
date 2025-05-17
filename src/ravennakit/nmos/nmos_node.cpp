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

#include "ravennakit/nmos/models/nmos_error.hpp"

#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/lexical_cast.hpp>
#include <utility>

namespace {

/**
 * Sets the default headers for the response.
 * Warning: these headers are probably not suitable for production use, see:
 * https://specs.amwa.tv/is-04/releases/v1.3.3/docs/APIs_-_Server_Side_Implementation_Notes.html#cross-origin-resource-sharing-cors
 * @param res
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
    boost::beast::http::response<boost::beast::http::string_body>& res, const boost::beast::http::status status,
    const std::string& error, const std::string& debug
) {
    res.result(status);
    set_default_headers(res);
    res.body() =
        boost::json::serialize(boost::json::value_from(rav::nmos::Error {static_cast<unsigned>(status), error, debug}));
    res.prepare_payload();
}

/**
 * Sets the error response for an invalid API version.
 * @param res The response to set.
 */
void invalid_api_version_response(boost::beast::http::response<boost::beast::http::string_body>& res) {
    set_error_response(
        res, boost::beast::http::status::bad_request, "Invalid API version",
        "Failed to parse a valid version in the form of vMAJOR.MINOR"
    );
}

/**
 * Sets the error response for an unsupported API version.
 * @param res The response to set.
 * @param version The unsupported API version.
 */
void version_not_supported_response(
    boost::beast::http::response<boost::beast::http::string_body>& res, const rav::nmos::ApiVersion& version
) {
    set_error_response(
        res, boost::beast::http::status::not_found, "Version not found",
        fmt::format("Version {} is not supported", version)
    );
}

/**
 * Sets the response to indicate that the request was successful and optionally adds the body (if not empty).
 * @param res The response to set.
 * @param body The body of the response.
 */
void ok_response(boost::beast::http::response<boost::beast::http::string_body>& res, std::string body) {
    res.result(boost::beast::http::status::ok);
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

boost::system::result<void, rav::nmos::Node::Error> rav::nmos::Node::Configuration::validate() const {
    if (operation_mode == OperationMode::registered_p2p) {
        if (discover_mode == DiscoverMode::dns || discover_mode == DiscoverMode::mdns) {
            return {};
        }
        // Unicast DNS and manual mode are not valid in registered_p2p mode because they are not valid for p2p
        return Error::incompatible_discover_mode;
    }

    if (operation_mode == OperationMode::registered) {
        if (discover_mode == DiscoverMode::dns || discover_mode == DiscoverMode::udns
            || discover_mode == DiscoverMode::mdns) {
            return {};
        }
        if (discover_mode == DiscoverMode::manual) {
            if (registry_address.empty()) {
                return Error::invalid_registry_address;
            }
            return {};
        }
        return Error::incompatible_discover_mode;
    }

    if (operation_mode == OperationMode::p2p) {
        if (discover_mode == DiscoverMode::mdns) {
            return {};
        }
        // Unicast DNS and manual mode are not valid in p2p mode
        return Error::incompatible_discover_mode;
    }

    RAV_ASSERT_FALSE("Should not have reached this line");
    return {};
}

rav::nmos::Node::Node(boost::asio::io_context& io_context) : http_server_(io_context) {
    http_server_.get("/", [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        ok_response(res, boost::json::serialize(boost::json::array({"x-nmos/"})));
    });

    http_server_.get("/x-nmos", [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        ok_response(res, boost::json::serialize(boost::json::array({"node/"})));
    });

    http_server_.get(
        "/x-nmos/node",
        [this](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
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
        [this](const HttpServer::Request&, HttpServer::Response& res, const PathMatcher::Parameters& params) {
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
                set_error_response(
                    res, boost::beast::http::status::bad_request, "Invalid device ID", "Device ID is empty"
                );
                return;
            }

            const auto uuid = boost::lexical_cast<boost::uuids::uuid>(*uuid_str);
            if (uuid.is_nil()) {
                set_error_response(
                    res, boost::beast::http::status::bad_request, "Invalid device ID", "Device ID is not a valid UUID"
                );
                return;
            }

            if (auto* device = get_device(uuid)) {
                ok_response(res, boost::json::serialize(boost::json::value_from(*device)));
                return;
            }

            set_error_response(res, boost::beast::http::status::not_found, "Not found", "Device not found");
        }
    );

    http_server_.get("/**", [](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        set_error_response(res, boost::beast::http::status::not_found, "Not found", "No matching route");
    });
}

boost::system::result<void> rav::nmos::Node::start(const std::string_view bind_address, const uint16_t port) {
    return http_server_.start(bind_address, port);
}

void rav::nmos::Node::stop() {
    http_server_.stop();
}

boost::asio::ip::tcp::endpoint rav::nmos::Node::get_local_endpoint() const {
    return http_server_.get_local_endpoint();
}

bool rav::nmos::Node::set_device(Device device) {
    if (device.id.is_nil()) {
        RAV_ERROR("Device ID should not be nil");
        return false;
    }

    device.node_id = uuid_;

    for (auto& existing_device : devices_) {
        if (existing_device.id == device.id) {
            existing_device = std::move(device);
            return true;
        }
    }

    devices_.push_back(std::move(device));

    return true;
}

const rav::nmos::Device* rav::nmos::Node::get_device(boost::uuids::uuid uuid) const {
    const auto it = std::find_if(devices_.begin(), devices_.end(), [uuid](const Device& device) {
        return device.id == uuid;
    });
    if (it != devices_.end()) {
        return &*it;
    }
    return nullptr;
}

const boost::uuids::uuid& rav::nmos::Node::get_uuid() const {
    return uuid_;
}

const std::vector<rav::nmos::Device>& rav::nmos::Node::get_devices() const {
    return devices_;
}

std::ostream& rav::nmos::operator<<(std::ostream& os, const Node::Error error) {
    switch (error) {
        case Node::Error::incompatible_discover_mode:
            os << "incompatible_discover_mode";
            break;
        case Node::Error::invalid_registry_address:
            os << "invalid_registry_address";
            break;
    }
    return os;
}

std::ostream& rav::nmos::operator<<(std::ostream& os, const Node::OperationMode operation_mode) {
    switch (operation_mode) {
        case Node::OperationMode::registered_p2p:
            os << "registered_p2p";
            break;
        case Node::OperationMode::registered:
            os << "registered";
            break;
        case Node::OperationMode::p2p:
            os << "p2p";
            break;
    }
    return os;
}

std::ostream& rav::nmos::operator<<(std::ostream& os, const Node::DiscoverMode discover_mode) {
    switch (discover_mode) {
        case Node::DiscoverMode::dns:
            os << "dns";
            break;
        case Node::DiscoverMode::udns:
            os << "udns";
            break;
        case Node::DiscoverMode::mdns:
            os << "mdns";
            break;
        case Node::DiscoverMode::manual:
            os << "manual";
            break;
    }
    return os;
}
