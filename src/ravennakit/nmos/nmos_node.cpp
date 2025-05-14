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
    http_server_.get("/", [this](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        boost::json::array array;
        array.push_back("x-nmos/");

        res.result(boost::beast::http::status::ok);
        res.set("Content-Type", "application/json");
        res.body() = boost::json::serialize(array);
        res.prepare_payload();
    });

    http_server_.get("/x-nmos", [this](const HttpServer::Request&, HttpServer::Response& res, PathMatcher::Parameters&) {
        boost::json::array array;
        array.push_back("node/");

        res.result(boost::beast::http::status::ok);
        res.set("Content-Type", "application/json");
        res.body() = boost::json::serialize(array);
        res.prepare_payload();
    });

    http_server_.get("**", [this](const HttpServer::Request& req, HttpServer::Response& res, PathMatcher::Parameters&) {
        handle_request(boost::beast::http::verb::get, req.target(), req, res);
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

namespace {

void set_error_response(
    const boost::beast::http::status status, const std::string& error, const std::string& debug,
    boost::beast::http::response<boost::beast::http::string_body>& res
) {
    res.result(status);
    res.set("Content-Type", "application/json");
    res.body() =
        boost::json::serialize(boost::json::value_from(rav::nmos::Error {static_cast<unsigned>(status), error, debug}));
    res.prepare_payload();
}

}  // namespace

void rav::nmos::Node::handle_request(
    boost::beast::http::verb method, const std::string_view target,
    const boost::beast::http::request<boost::beast::http::string_body>& req,
    boost::beast::http::response<boost::beast::http::string_body>& res
) {
    StringParser parser(target);

    if (!parser.skip("/x-nmos")) {
        set_error_response(boost::beast::http::status::not_found, "Not found", "x-nmos not found", res);
        return;
    }

    parser.skip('/'); // Optional

    const auto api = parser.read_until('/');
    if (!api || api->empty()) {
        set_error_response(boost::beast::http::status::not_found, "Not found", "api type not found", res);
        return;
    }

    if (api != "node") {
        set_error_response(
            boost::beast::http::status::not_found, "Not found", fmt::format("unsupported api: {}", *api), res
        );
        return;
    }

    const auto version_str = parser.read_until('/');
    if (!version_str || version_str->empty()) {
        set_error_response(boost::beast::http::status::not_found, "Not found", "version not found", res);
        return;
    }

    const auto version = ApiVersion::from_string(*version_str);
    if (!version) {
        set_error_response(boost::beast::http::status::not_found, "Not found", "failed to parse version", res);
        return;
    }

    if (parser.exhausted()) {
        serve_root(*version, res);
        return;
    }

    set_error_response(boost::beast::http::status::not_found, "Not found", "unknown target", res);
}

void rav::nmos::Node::serve_root(ApiVersion api_version, HttpServer::Response& res) {
    res.result(boost::beast::http::status::ok);
    res.set("Content-Type", "application/json");

    boost::json::array array;
    array.push_back("self/");
    array.push_back("sources/");
    array.push_back("flows/");
    array.push_back("devices/");
    array.push_back("senders/");
    array.push_back("receivers/");

    res.body() = boost::json::serialize(array);
    res.prepare_payload();
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
