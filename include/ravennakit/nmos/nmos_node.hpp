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
#include "detail/nmos_error.hpp"
#include "detail/nmos_operating_mode.hpp"
#include "detail/nmos_registry_browser.hpp"
#include "models/nmos_device.hpp"
#include "models/nmos_flow.hpp"
#include "models/nmos_receiver.hpp"
#include "models/nmos_self.hpp"
#include "models/nmos_sender.hpp"
#include "models/nmos_source.hpp"
#include "ravennakit/core/json.hpp"
#include "ravennakit/core/net/http/http_client.hpp"
#include "ravennakit/core/net/http/http_server.hpp"
#include "ravennakit/core/net/interfaces/network_interface_config.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"

#include <boost/uuid.hpp>
#include <boost/system/result.hpp>
#include <fmt/ostream.h>

namespace rav::nmos {

/**
 * Implements the NMOS node as defined in the NMOS specifications.
 * https://specs.amwa.tv/nmos/branches/main/docs/Technical_Overview.html#nmos-model-and-terminology
 */
class Node: public ptp::Instance::Subscriber {
  public:
    static std::array<ApiVersion, 2> k_supported_api_versions;
    static constexpr auto k_default_timeout = std::chrono::milliseconds(2000);
    static constexpr auto k_internal_clock_name = "clk0";
    static constexpr auto k_ptp_clock_name = "clk1";

    /**
     * The configuration of the NMOS node.
     */
    struct Configuration {
        boost::uuids::uuid id;  // The UUID of the NMOS node.
        OperationMode operation_mode {OperationMode::mdns_p2p};
        ApiVersion api_version {ApiVersion::v1_3()};
        std::string registry_address;  // For when operation_mode is registered and discover_mode is manual.
        bool enabled {false};          // Whether the node is enabled or not.
        uint16_t node_api_port {0};    // The port of the local node API.
        std::string label;             // Freeform string label for the resource.
        std::string description;       // Detailed description of the resource.

        /**
         * Checks if the configuration is semantically valid, return a message if not.
         * @return A result indicating whether the configuration is valid or not.
         */
        [[nodiscard]] boost::system::result<void, Error> validate() const;

        /**
         * @return The configuration as a JSON object.
         */
        [[nodiscard]] boost::json::value to_json() const;

        /**
         * Creates a configuration object from a JSON object.
         * @param json The JSON object to convert.
         * @return A configuration object if the JSON is valid, otherwise an error message.
         */
        static boost::system::result<Configuration, std::string> from_json(const boost::json::value& json);

        [[nodiscard]] auto constexpr tie() const {
            return std::tie(id, operation_mode, api_version, registry_address, enabled, label, description);
        }

        friend bool operator==(const Configuration& lhs, const Configuration& rhs) {
            return lhs.tie() == rhs.tie();
        }

        friend bool operator!=(const Configuration& lhs, const Configuration& rhs) {
            return lhs.tie() != rhs.tie();
        }
    };

    struct RegistryInfo {
        std::string name;
        std::string address;
    };

    enum class Status { disabled, discovering, connecting, connected, registered, p2p, error };

    SafeFunction<void(const Status& status, const RegistryInfo& registry_info)> on_status_changed;
    SafeFunction<void(const Configuration& config)> on_configuration_changed;

    explicit Node(
        boost::asio::io_context& io_context, ptp::Instance& ptp_instance,
        std::unique_ptr<RegistryBrowserBase> registry_browser = nullptr,
        std::unique_ptr<HttpClientBase> http_client = nullptr
    );

    ~Node() override;

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
     * Sets the configuration of the NMOS node.
     * @param new_configuration The configuration to update.
     * @param force_update Whether to force the update even if the configuration didn't change.
     */
    void set_configuration(Configuration new_configuration, bool force_update = false);

    /**
     * @return The current configuration of the NMOS node.
     */
    [[nodiscard]] const Configuration& get_configuration() const;

    /**
     * @return The local (listening) endpoint of the server.
     */
    [[nodiscard]] boost::asio::ip::tcp::endpoint get_local_endpoint() const;

    /**
     * Adds the given device to the node or updates an existing device if it already exists (based on the uuid).
     * The node if of the device is set to the node's uuid.
     * @param device The device to set or update.
     */
    [[nodiscard]] bool add_or_update_device(Device device);

    /**
     * Finds a device by its uuid.
     * @param uuid The uuid of the device to find.
     * @return A pointer to the device if found, or nullptr if not found.
     */
    [[nodiscard]] const Device* find_device(const boost::uuids::uuid& uuid) const;

    /**
     * Removes a device from the node by its uuid. Resources associated with the device will be removed as well.
     * @param uuid The uuid of the device to remove.
     * @return True if the device was removed successfully, false otherwise.
     */
    [[nodiscard]] bool remove_device(boost::uuids::uuid uuid);

    /**
     * Adds the given flow to the node or updates an existing flow if it already exists (based on the uuid).
     * @param flow The flow to set.
     * @return True if the flow was set successfully, false otherwise.
     */
    [[nodiscard]] bool add_or_update_flow(Flow flow);

    /**
     * Finds a flow by its uuid.
     * @param uuid The uuid of the flow to find.
     * @return A pointer to the flow if found, or nullptr if not found.
     */
    [[nodiscard]] const Flow* find_flow(const boost::uuids::uuid& uuid) const;

    /**
     * Removes a flow from the node by its uuid.
     * @param uuid The uuid of the flow to remove.
     * @return True if the flow was removed successfully, false otherwise.
     */
    [[nodiscard]] bool remove_flow(const boost::uuids::uuid& uuid);

    /**
     * Adds the given receiver to the node or updates an existing receiver if it already exists (based on the uuid).
     * @param receiver The receiver to set.
     * @return True if the receiver was set successfully, false otherwise.
     */
    [[nodiscard]] bool add_or_update_receiver(Receiver receiver);

    /**
     * Finds a receiver by its uuid.
     * @param uuid The uuid of the receiver to find.
     * @return A pointer to the receiver if found, or nullptr if not found.
     */
    [[nodiscard]] const Receiver* find_receiver(const boost::uuids::uuid& uuid) const;

    /**
     * Removes a receiver from the node by its uuid.
     * @param uuid The uuid of the receiver to remove.
     * @return True if the device was removed successfully, false otherwise.
     */
    [[nodiscard]] bool remove_receiver(boost::uuids::uuid uuid);

    /**
     * Adds the given sender to the node or updates an existing sender if it already exists (based on the uuid).
     * @param sender The sender to set.
     * @return True if the sender was set successfully, false otherwise.
     */
    [[nodiscard]] bool add_or_update_sender(Sender sender);

    /**
     * Finds a sender by its uuid.
     * @param uuid The uuid of the sender to find.
     * @return A pointer to the sender if found, or nullptr if not found.
     */
    [[nodiscard]] const Sender* find_sender(const boost::uuids::uuid& uuid) const;

    /**
     * Removes a sender from the node by its uuid.
     * @param uuid The uuid of the sender to remove.
     * @return True if the sender was removed successfully, false otherwise.
     */
    [[nodiscard]] bool remove_sender(boost::uuids::uuid uuid);

    /**
     * Adds the given source to the node or updates an existing source if it already exists (based on the uuid).
     * @param source The source to set.
     * @return True if the source was set successfully, false otherwise.
     */
    [[nodiscard]] bool add_or_update_source(Source source);

    /**
     * Finds a source by its uuid.
     * @param uuid The uuid of the source to find.
     * @return A pointer to the source if found, or nullptr if not found.
     */
    [[nodiscard]] const Source* find_source(const boost::uuids::uuid& uuid) const;

    /**
     * Removes a source from the node by its uuid.
     * @param uuid The uuid of the source to remove.
     * @return True if the source was removed successfully, false otherwise.
     */
    [[nodiscard]] bool remove_source(boost::uuids::uuid uuid);

    /**
     * @return The uuid of the node.
     */
    [[nodiscard]] const boost::uuids::uuid& get_uuid() const;

    /**
     * @return The list of devices in the node.
     */
    [[nodiscard]] const std::vector<Device>& get_devices() const;

    /**
     * @return The list of flows in the node.
     */
    [[nodiscard]] const std::vector<Flow>& get_flows() const;

    /**
     * @return The list of receivers in the node.
     */
    [[nodiscard]] const std::vector<Receiver>& get_receivers() const;

    /**
     * @return The list of senders in the node.
     */
    [[nodiscard]] const std::vector<Sender>& get_senders() const;

    /**
     * @return  The list of sources in the node.
     */
    [[nodiscard]] const std::vector<Source>& get_sources() const;

    /**
     * @return The current state.
     */
    [[nodiscard]] const Status& get_status() const;

    /**
     * @return The current registry information.
     */
    [[nodiscard]] const RegistryInfo& get_registry_info() const;

    /**
     * Updates the node based on given network interface configuration.
     * @param interface_config The network interface configuration to apply.
     */
    void set_network_interface_config(const NetworkInterfaceConfig& interface_config);

    /**
     * @param version The API version to check.
     * @return The index of the supported API version if it exists, or std::nullopt if not found.
     */
    [[nodiscard]] static std::optional<size_t> index_of_supported_api_version(const ApiVersion& version);

    void ptp_parent_changed(const ptp::ParentDs& parent) override;
    void ptp_port_changed_state(const ptp::Port& port) override;

  private:
    static constexpr uint8_t k_max_failed_heartbeats = 5;
    static constexpr auto k_heartbeat_interval = std::chrono::seconds(5);
    static constexpr size_t k_clock_ptp_index = 1;

    ptp::Instance& ptp_instance_;
    Self self_;
    std::vector<Device> devices_;
    std::vector<Flow> flows_;
    std::vector<Receiver> receivers_;
    std::vector<Sender> senders_;
    std::vector<Source> sources_;

    Configuration configuration_;
    Status status_ {Status::disabled};
    int post_resource_error_count_ = 0;

    std::optional<dnssd::ServiceDescription> selected_registry_;
    RegistryInfo registry_info_;

    HttpServer http_server_;
    std::unique_ptr<HttpClientBase> http_client_;
    std::unique_ptr<RegistryBrowserBase> registry_browser_;
    AsioTimer timer_;

    uint8_t failed_heartbeat_count_ = 0;
    AsioTimer heartbeat_timer_;  // Keep below connector_ to avoid dangling reference
    Version current_version_;

    [[nodiscard]] boost::system::result<void, Error> start_internal();
    void stop_internal();

    void register_async();
    void unregister_async();
    void post_resource_async(std::string type, boost::json::value resource);
    void delete_resource_async(std::string resource_type, const boost::uuids::uuid& id);
    void update_self();
    void send_heartbeat_async();
    void connect_to_registry_async();
    void connect_to_registry_async(std::string_view host, std::string_view service);

    [[nodiscard]] bool add_receiver_to_device(const Receiver& receiver);
    [[nodiscard]] bool add_sender_to_device(const Sender& sender);

    bool select_registry(const dnssd::ServiceDescription& desc);
    void handle_registry_discovered(const dnssd::ServiceDescription& desc);

    void set_status(Status new_status);
    void update_all_resources_to_now();
    void send_updated_resources_async();
};

const char* to_string(const Node::Status& status);

}  // namespace rav::nmos
