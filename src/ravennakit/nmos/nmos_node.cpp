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

void set_default_headers(rav::HttpServer::Response& res) {
    res.set("Content-Type", "application/json");
    res.set("Access-Control-Allow-Origin", "*");
    res.set("Access-Control-Allow-Methods", "GET, PUT, POST, PATCH, HEAD, OPTIONS, DELETE");
    res.set("Access-Control-Allow-Headers", "Content-Type, Accept");
    res.set("Access-Control-Max-Age", "3600");
}

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

void invalid_api_version_response(boost::beast::http::response<boost::beast::http::string_body>& res) {
    set_error_response(
        res, boost::beast::http::status::bad_request, "Invalid API version",
        "Failed to parse a valid version in the form of vMAJOR.MINOR"
    );
}

void version_not_supported_response(
    boost::beast::http::response<boost::beast::http::string_body>& res, const rav::nmos::ApiVersion& version
) {
    set_error_response(
        res, boost::beast::http::status::not_found, "Version not found",
        fmt::format("Version {} is not supported", version)
    );
}

void ok_response(boost::beast::http::response<boost::beast::http::string_body>& res, std::string body) {
    res.result(boost::beast::http::status::ok);
    set_default_headers(res);
    res.body() = std::move(body);
    res.prepare_payload();
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
        "/x-nmos/node/**",
        [this](const HttpServer::Request& req, HttpServer::Response& res, const PathMatcher::Parameters&) {
            node_api_root(req, res);
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

bool rav::nmos::Node::is_version_supported(const ApiVersion& version) {
    return std::any_of(
        k_supported_api_versions.begin(), k_supported_api_versions.end(),
        [&version](const auto& supported_version) {
            return supported_version == version;
        }
    );
}

void rav::nmos::Node::node_api_root(const HttpServer::Request& req, HttpServer::Response& res) {
    const boost::urls::url_view target(req.target());

    const auto segments = target.segments();
    auto segment = segments.begin();

    auto next = [&] {
        ++segment;
        return segment != segments.end() && !(*segment).empty();
    };

    if (segment == segments.end() || *segment != "x-nmos") {
        set_error_response(
            res, boost::beast::http::status::bad_request, "Invalid target",
            "The request was incorrectly routed to the node API"
        );
        return;
    }

    // Expect: /node
    if (!next() || *segment != "node") {
        set_error_response(
            res, boost::beast::http::status::bad_request, "Invalid target",
            "The request was incorrectly routed to the node API"
        );
        return;
    }

    // Serve /x-nmos/node
    if (!next()) {
        boost::json::array versions;
        for (const auto& version : k_supported_api_versions) {
            versions.push_back({fmt::format("{}/", version.to_string())});
        }
        ok_response(res, boost::json::serialize(versions));
        return;
    }

    const auto api_version = ApiVersion::from_string(*segment);
    if (!api_version) {
        invalid_api_version_response(res);
        return;
    }

    if (!is_version_supported(*api_version)) {
        version_not_supported_response(res, *api_version);
        return;
    }

    // Serve /x-nmos/node/{version}
    if (!next()) {
        ok_response(
            res,
            boost::json::serialize(
                boost::json::array({"self/", "sources/", "flows/", "devices/", "senders/", "receivers/"})
            )
        );
        return;
    }

    // Serve /x-nmos/node/{version}/devices
    if (*segment == "devices") {
        if (!next()) {
            ok_response(res, boost::json::serialize(boost::json::value_from(devices_)));
            return;
        }

        const auto uuid = boost::lexical_cast<boost::uuids::uuid>(*segment);
        if (auto* device = get_device(uuid)) {
            ok_response(res, boost::json::serialize(boost::json::value_from(*device)));
            return;
        }

        set_error_response(res, boost::beast::http::status::not_found, "Not found", "Device not found");
        return;
    }

    set_error_response(res, boost::beast::http::status::not_found, "Not found", "No matching route");
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
