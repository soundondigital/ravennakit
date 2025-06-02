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
#include "detail/nmos_connector.hpp"
#include "detail/nmos_discover_mode.hpp"
#include "detail/nmos_error.hpp"
#include "detail/nmos_operating_mode.hpp"
#include "detail/nmos_registry_browser.hpp"
#include "models/nmos_device.hpp"
#include "models/nmos_flow.hpp"
#include "models/nmos_receiver.hpp"
#include "models/nmos_self.hpp"
#include "models/nmos_sender.hpp"
#include "models/nmos_source.hpp"
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
     * The configuration of the NMOS node.
     */
    struct Configuration {
        OperationMode operation_mode {OperationMode::registered_p2p};
        DiscoverMode discover_mode {DiscoverMode::dns};
        ApiVersion api_version {ApiVersion::v1_3()};
        std::string registry_address;  // For when operation_mode is registered and discover_mode is manual.
        bool enabled {false};          // Whether the node is enabled or not.
        uint16_t node_api_port {0};    // The port of the local node API.

        /**
         * Checks if the configuration is semantically valid, return a message if not.
         * @return A result indicating whether the configuration is valid or not.
         */
        [[nodiscard]] boost::system::result<void, Error> validate() const;

        [[nodiscard]] auto constexpr tie() const {
            return std::tie(operation_mode, discover_mode, api_version, registry_address, enabled);
        }

        friend bool operator==(const Configuration& lhs, const Configuration& rhs) {
            return lhs.tie() == rhs.tie();
        }

        friend bool operator!=(const Configuration& lhs, const Configuration& rhs) {
            return lhs.tie() != rhs.tie();
        }
    };

    /**
     * Struct for updating the configuration of the NMOS node. Only the fields that are set are taken into account,
     * which allows for partial updates.
     */
    struct ConfigurationUpdate {
        std::optional<OperationMode> operation_mode;
        std::optional<DiscoverMode> discover_mode;
        std::optional<ApiVersion> api_version;
        std::optional<std::string> registry_address;
        std::optional<bool> enabled;
        std::optional<uint16_t> node_api_port;

        void apply_to_config(Configuration& config) const;
    };

    struct State {
        /// Whether the node is operating in registered or p2p mode.
        OperationMode operation_mode {};
        /// Whether the node found a registry using unicast DNS or multicast DNS.
        DiscoverMode discover_mode {};
        /// The port of the local node api.
        uint16_t node_api_port {};
        /// Whether the node is currently registered with a registry or not.
        bool is_registered {};
    };

    explicit Node(boost::asio::io_context& io_context, const ConfigurationUpdate& configuration = {});

    /**
     * Starts the services of this node (HTTP server, advertisements, etc.).
     * @return An error code if the node fails to start, or an empty result if it succeeds.
     */
    [[nodiscard]] boost::system::result<void, Error> start();

    /**
     * Stops all the operations of this node (HTTP server, advertisements, etc.).
     */
    void stop();

    /**
     * Updates the configuration of the NMOS node. Only takes into account the fields in the configuration that are set.
     * This allows updating only a subset of the configuration.
     * @param update The configuration to update.
     * @param force_update Whether to force the update even if the configuration didn't change.
     */
    [[nodiscard]] boost::system::result<void, Error> set_configuration(const ConfigurationUpdate& update, bool force_update = false);

    /**
     * @return The local (listening) endpoint of the server.
     */
    [[nodiscard]] boost::asio::ip::tcp::endpoint get_local_endpoint() const;

    /**
     * Adds the given device to the node or updates an existing device if it already exists (based on the uuid).
     * The node if of the device is set to the node's uuid.
     * @param device The device to set or update.
     */
    [[nodiscard]] bool set_device(Device device);

    /**
     * Finds a device by its uuid.
     * @param uuid The uuid of the device to find.
     * @return A pointer to the device if found, or nullptr if not found.
     */
    [[nodiscard]] const Device* get_device(boost::uuids::uuid uuid) const;

    /**
     * Adds the given flow to the node or updates an existing flow if it already exists (based on the uuid).
     * @param flow The flow to set.
     * @return True if the flow was set successfully, false otherwise.
     */
    [[nodiscard]] bool set_flow(Flow flow);

    /**
     * Finds a flow by its uuid.
     * @param uuid The uuid of the flow to find.
     * @return A pointer to the flow if found, or nullptr if not found.
     */
    [[nodiscard]] const Flow* get_flow(boost::uuids::uuid uuid) const;

    /**
     * Adds the given receiver to the node or updates an existing receiver if it already exists (based on the uuid).
     * @param receiver The receiver to set.
     * @return True if the receiver was set successfully, false otherwise.
     */
    [[nodiscard]] bool set_receiver(Receiver receiver);

    /**
     * Finds a receiver by its uuid.
     * @param uuid The uuid of the receiver to find.
     * @return A pointer to the receiver if found, or nullptr if not found.
     */
    [[nodiscard]] const Receiver* get_receiver(boost::uuids::uuid uuid) const;

    /**
     * Adds the given sender to the node or updates an existing sender if it already exists (based on the uuid).
     * @param sender The sender to set.
     * @return True if the sender was set successfully, false otherwise.
     */
    [[nodiscard]] bool set_sender(Sender sender);

    /**
     * Finds a sender by its uuid.
     * @param uuid The uuid of the sender to find.
     * @return A pointer to the sender if found, or nullptr if not found.
     */
    [[nodiscard]] const Sender* get_sender(boost::uuids::uuid uuid) const;

    /**
     * Adds the given source to the node or updates an existing source if it already exists (based on the uuid).
     * @param source The source to set.
     * @return True if the source was set successfully, false otherwise.
     */
    [[nodiscard]] bool set_source(Source source);

    /**
     * Finds a source by its uuid.
     * @param uuid The uuid of the source to find.
     * @return A pointer to the source if found, or nullptr if not found.
     */
    [[nodiscard]] const Source* get_source(boost::uuids::uuid uuid) const;

    /**
     * @return The uuid of the node.
     */
    [[nodiscard]] const boost::uuids::uuid& get_uuid() const;

    /**
     * @return The list of devices in the node.
     */
    [[nodiscard]] const std::vector<Device>& get_devices() const;

  private:
    static constexpr uint8_t k_max_failed_heartbeats = 5;
    static constexpr auto k_heartbeat_interval = std::chrono::seconds(5);

    Self self_;
    std::vector<Device> devices_;
    std::vector<Flow> flows_;
    std::vector<Receiver> receivers_;
    std::vector<Sender> senders_;
    std::vector<Source> sources_;

    Configuration configuration_;
    State state_;

    HttpServer http_server_;
    Connector connector_;

    uint8_t failed_heartbeat_count_ = 0;
    AsioTimer heartbeat_timer_;  // Keep below connector_ to avoid dangling reference

    /**
     * Starts the services of this node (HTTP server, advertisements, etc.).
     * @return An error code if the node fails to start, or an empty result if it succeeds.
     */
    [[nodiscard]] boost::system::result<void, Error> start_internal();

    /**
     * Stops all the operations of this node (HTTP server, advertisements, etc.).
     */
    void stop_internal();

    void register_async();
    void post_resource_async(std::string type, boost::json::value resource);
    void send_heartbeat_async();

    [[nodiscard]] bool add_receiver_to_device(const Receiver& receiver);
    [[nodiscard]] bool add_sender_to_device(const Sender& sender);
};

}  // namespace rav::nmos
