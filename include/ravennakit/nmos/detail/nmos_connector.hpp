/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "nmos_api_version.hpp"
#include "nmos_discover_mode.hpp"
#include "nmos_operating_mode.hpp"
#include "nmos_registry_browser.hpp"
#include "ravennakit/core/net/http/http_client.hpp"
#include "ravennakit/core/util/safe_function.hpp"
#include "ravennakit/core/util/todo.hpp"

#include <boost/asio.hpp>
#include <boost/json/serialize.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace rav::nmos {

/**
 * Finds and maintains a connection to an NMOS registry, or finds and connects to nodes when p2p is enabled.
 */
class Connector {
  public:
    static constexpr auto k_default_timeout = std::chrono::milliseconds(2000);
    static constexpr uint8_t k_max_failed_heartbeats = 5;
    static constexpr auto k_heartbeat_interval = std::chrono::seconds(5);

    enum class Status {
        idle,
        connected,
        disconnected,
        p2p,
    };

    SafeFunction<void(Status status)> on_status_changed;

    explicit Connector(boost::asio::io_context& io_context) :
        registry_browser_(io_context), http_client_(io_context), timer_(io_context) {}

    void start(
        const OperationMode operation_mode, const DiscoverMode discover_mode, const ApiVersion api_version,
        const std::string& registry_address
    ) {
        operation_mode_ = operation_mode;
        api_version_ = api_version;
        selected_registry_.reset();

        timer_.stop();

        if (discover_mode == DiscoverMode::manual) {
            RAV_ASSERT(
                operation_mode == OperationMode::registered, "When connecting manually only registered mode is allowed"
            );

            if (registry_address.empty()) {
                RAV_ERROR("Registry address is empty");
                return;
            }

            const boost::urls::url_view url(registry_address);
            // connect_to_registry_async(url.host(), url.port_number());
            // TODO: connect to registry using the provided address
            return;
        }

        if (operation_mode == OperationMode::p2p) {
            selected_registry_.reset();
            registry_browser_.stop();
            set_status(Status::p2p);
            return;
        }

        // All other cases require a timeout to wait for the registry to be discovered

        registry_browser_.on_registry_discovered.reset();
        registry_browser_.start(discover_mode, api_version);

        timer_.once(k_default_timeout, [this] {
            // Subscribe to future registry discoveries
            registry_browser_.on_registry_discovered = [this](const dnssd::ServiceDescription& desc) {
                handle_registry_discovered(desc);
            };
            if (const auto reg = registry_browser_.find_most_suitable_registry()) {
                select_registry(*reg);
            } else if (operation_mode_ == OperationMode::registered_p2p) {
                set_status(Status::p2p);
            } else {
                set_status(Status::idle);
            }
        });
    }

    void stop() {
        timer_.stop();
        registry_browser_.stop();
        set_status(Status::idle);
    }

    void post_async(
        const std::string_view target, std::string body, HttpClient::CallbackType callback,
        const std::string_view content_type = "application/json"
    ) {
        http_client_.post_async(target, std::move(body), std::move(callback), content_type);
    }

    /**
     * Clears all scheduled requests if there are any. Otherwise, this function has no effect.
     */
    void cancel_outstanding_requests() {
        http_client_.cancel_outstanding_requests();
    }

  private:
    OperationMode operation_mode_ = OperationMode::registered_p2p;
    ApiVersion api_version_ = ApiVersion::v1_3();
    Status status_ = Status::idle;
    std::optional<dnssd::ServiceDescription> selected_registry_;
    RegistryBrowser registry_browser_;
    HttpClient http_client_;
    AsioTimer timer_;

    void handle_registry_discovered(const dnssd::ServiceDescription& desc) {
        RAV_INFO("Discovered NMOS registry: {}", desc.to_string());
        if (operation_mode_ == OperationMode::registered_p2p || operation_mode_ == OperationMode::registered) {
            select_registry(desc);
        }
    }

    bool select_registry(const dnssd::ServiceDescription& desc) {
        if (selected_registry_ && selected_registry_->host_target == desc.host_target
            && selected_registry_->port == desc.port) {
            return false;  // Already connected to this registry
        }
        selected_registry_ = desc;
        connect_to_registry_async();
        return true;  // Successfully selected a new registry
    }

    void connect_to_registry_async() {
        if (!selected_registry_.has_value()) {
            RAV_ASSERT_FALSE("A registry must be selected at this point");
            return;  // No registry selected
        }
        http_client_.set_host(selected_registry_->host_target, std::to_string(selected_registry_->port));
        http_client_.get_async("/", [this](const boost::system::result<http::response<http::string_body>>& result) {
            if (result.has_error()) {
                RAV_ERROR("Error connecting to NMOS registry: {}", result.error().message());
                set_status(Status::disconnected);
            } else if (result.value().result() != http::status::ok) {
                RAV_ERROR("Unexpected response from NMOS registry: {}", result.value().result_int());
                set_status(Status::disconnected);
            } else {
                set_status(Status::connected);
            }
        });
    }

    void set_status(const Status status) {
        if (status_ == status) {
            return;  // No change in status
        }

        status_ = status;

        switch (status) {
            case Status::connected: {
                RAV_ASSERT(selected_registry_.has_value(), "No registry selected");
                RAV_INFO(
                    "Connected to NMOS registry {} at {}:{}", selected_registry_->name, selected_registry_->host_target,
                    selected_registry_->port
                );
                break;
            }
            case Status::disconnected: {
                RAV_INFO(
                    "Disconnected from NMOS registry at {}:{}", http_client_.get_host(), http_client_.get_service()
                );
                break;
            }
            case Status::p2p:
                if (operation_mode_ == OperationMode::p2p) {
                    RAV_INFO("Switching to p2p mode");
                } else {
                    RAV_INFO("Falling back to p2p mode, registry not available");
                }
                break;
            case Status::idle:
            default: {
                RAV_INFO("NMOS Connector status changed to {}", status);
                break;
            }
        }

        on_status_changed(status);
    }
};

/// Overload the output stream operator for the Node::Error enum class
inline std::ostream& operator<<(std::ostream& os, const Connector::Status status) {
    switch (status) {
        case Connector::Status::p2p:
            os << "p2p";
            break;
        case Connector::Status::idle:
            os << "idle";
            break;
        case Connector::Status::connected:
            os << "connected";
            break;
        case Connector::Status::disconnected:
            os << "disconnected";
            break;
    }
    return os;
}

}  // namespace rav::nmos

/// Make Connector::Status printable with fmt
template<>
struct fmt::formatter<rav::nmos::Connector::Status>: ostream_formatter {};
