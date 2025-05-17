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

#include "detail/nmos_api_version.hpp"
#include "models/nmos_device.hpp"
#include "models/nmos_self.hpp"
#include "ravennakit/core/net/http/http_server.hpp"

#include <boost/uuid.hpp>
#include <boost/system/result.hpp>
#include <fmt/ostream.h>

namespace rav::nmos {

/**
 * Implements the NMOS node as defined in the NMOS specifications.
 * https://specs.amwa.tv/nmos/branches/main/docs/Technical_Overview.html#nmos-model-and-terminology
 */
class Node {
  public:
    static std::array<ApiVersion, 2> k_supported_api_versions;

    /**
     * Errors used in the NMOS node.
     */
    enum class Error {
        incompatible_discover_mode,
        invalid_registry_address,
    };

    /**
     * The mode of discovery for the NMOS node, for both registry and peer-to-peer discovery.
     */
    enum class DiscoverMode {
        /// The node will use both multicast and unicast DNS to discover registries.
        dns,
        /// The node will use unicast DNS to discover registries.
        udns,
        /// The node will use multicast DNS to discover registries or other nodes.
        mdns,
        /// The node will connect to the manually specified address and port and not discover anything.
        manual,
    };

    /**
     * The mode of operation for the NMOS node.
     */
    enum class OperationMode {
        /// The node registers itself with a registry or falls back to peer-to-peer discovery if no registry is
        /// available. When a registry comes online during p2p operation, the node will switch to registered mode.
        registered_p2p,
        /// The node registers itself with a registry and does not use peer-to-peer discovery.
        registered,
        /// The node only uses peer-to-peer discovery and does not register with a registry.
        p2p,
    };

    /**
     * The configuration of the NMOS node.
     */
    struct Configuration {
        OperationMode operation_mode {OperationMode::registered_p2p};
        DiscoverMode discover_mode {DiscoverMode::dns};
        std::string registry_address;  // Necessary for when operation_mode is registered and discover_mode is manual.

        /**
         * Checks if the configuration is semantically valid, return a message if not.
         * @return A result indicating whether the configuration is valid or not.
         */
        [[nodiscard]] boost::system::result<void, Error> validate() const;

        [[nodiscard]] auto constexpr tie() const {
            return std::tie(operation_mode, discover_mode, registry_address);
        }

        friend bool operator==(const Configuration& lhs, const Configuration& rhs) {
            return lhs.tie() == rhs.tie();
        }

        friend bool operator!=(const Configuration& lhs, const Configuration& rhs) {
            return lhs.tie() != rhs.tie();
        }
    };

    explicit Node(boost::asio::io_context& io_context);

    /**
     * Starts the services of this node (HTTP server, advertisements, etc.).
     *  @param bind_address The address to bind to.
     * @param port The port to bind to.
     * @return An error code if the node fails to start, or an empty result if it succeeds.
     */
    boost::system::result<void> start(std::string_view bind_address, uint16_t port);

    /**
     * Stops all the operations of this node (HTTP server, advertisements, etc.).
     */
    void stop();

    /**
     * @return The local (listening) endpoint of the server.
     */
    [[nodiscard]] boost::asio::ip::tcp::endpoint get_local_endpoint() const;

    /**
     * Adds the given device to the node or updates an existing device if it already exists (based on the uuid).
     * @param device The device to set or update.
     */
    [[nodiscard]] bool set_device(Device device);

    /**
     * Removes the device with the given uuid from the node.
     * @param uuid The uuid of the device to remove.
     */
    [[nodiscard]] const Device* get_device(boost::uuids::uuid uuid) const;

    /**
     * @return The uuid of the node.
     */
    [[nodiscard]] const boost::uuids::uuid& get_uuid() const;

    /**
     * @return The list of devices in the node.
     */
    [[nodiscard]] const std::vector<Device>& get_devices() const;

  private:
    HttpServer http_server_;
    Self self_;
    std::vector<Device> devices_;
};

/// Overload the output stream operator for the Node::Error enum class
std::ostream& operator<<(std::ostream& os, Node::Error error);

/// Overload the output stream operator for the Node::Error enum class
std::ostream& operator<<(std::ostream& os, Node::OperationMode operation_mode);

/// Overload the output stream operator for the Node::DiscoverMode enum class
std::ostream& operator<<(std::ostream& os, Node::DiscoverMode discover_mode);

}  // namespace rav::nmos

/// Make Node::Error printable with fmt
template<>
struct fmt::formatter<rav::nmos::Node::Error>: ostream_formatter {};

/// Make Node::OperationMode printable with fmt
template<>
struct fmt::formatter<rav::nmos::Node::OperationMode>: ostream_formatter {};

/// Make Node::DiscoverMode printable with fmt
template<>
struct fmt::formatter<rav::nmos::Node::DiscoverMode>: ostream_formatter {};
