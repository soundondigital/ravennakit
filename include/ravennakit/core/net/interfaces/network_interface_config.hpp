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
#include "network_interface.hpp"
#include "network_interface_list.hpp"
#include "ravennakit/core/json.hpp"
#include "ravennakit/core/util/rank.hpp"

#include <map>

namespace rav {

class NetworkInterfaceConfig {
  public:
    std::map<Rank, NetworkInterface::Identifier> interfaces;

    friend bool operator==(const NetworkInterfaceConfig& lhs, const NetworkInterfaceConfig& rhs) {
        return lhs.interfaces == rhs.interfaces;
    }

    friend bool operator!=(const NetworkInterfaceConfig& lhs, const NetworkInterfaceConfig& rhs) {
        return lhs.interfaces != rhs.interfaces;
    }

    /**
     * Sets a network interface by its rank.
     * @param rank The rank of the network interface.
     * @param identifier The identifier of the network interface.
     */
    void set_interface(const Rank rank, const NetworkInterface::Identifier& identifier) {
        interfaces[rank] = identifier;
    }

    /**
     * Gets a network interface by its rank.
     * @param rank The rank of the network interface.
     * @return A pointer to the network interface identifier if found, or nullptr if not found.
     */
    [[nodiscard]] const NetworkInterface::Identifier* get_interface_for_rank(const Rank rank) const {
        const auto it = interfaces.find(rank);
        if (it == interfaces.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /**
     * @return The first IPv4 address of one of the network interfaces. The address will be unspecified if the
     * interface is not found or if it has no IPv4 address.
     */
    [[nodiscard]] boost::asio::ip::address_v4 get_interface_ipv4_address(const Rank rank) const {
        const auto it = interfaces.find(rank);
        if (it == interfaces.end()) {
            return boost::asio::ip::address_v4 {};
        }
        if (auto* interface = NetworkInterfaceList::get_system_interfaces().get_interface(it->second)) {
            return interface->get_first_ipv4_address();
        }
        return boost::asio::ip::address_v4 {};
    }

    /**
     * @return A map of all network interfaces and their first IPv4 address. The address will be unspecified if the
     * interface has no IPv4 address.
     */
    [[nodiscard]] std::map<Rank, boost::asio::ip::address_v4> get_interface_ipv4_addresses() const {
        std::map<Rank, boost::asio::ip::address_v4> addresses;
        for (const auto& iface : interfaces) {
            addresses[iface.first] = get_interface_ipv4_address(iface.first);
        }
        return addresses;
    }

    /**
     * Checks if the configuration is empty.
     * @return True if there are no interfaces configured, false otherwise.
     */
    [[nodiscard]] bool empty() const {
        return interfaces.empty();
    }

    /**
     * @return A string representation of the network interface configuration.
     */
    [[nodiscard]] std::string to_string() const {
        std::string output = "Network interface configuration: ";
        if (interfaces.empty()) {
            return output + "none";
        }
        bool first = true;
        for (auto& iface : interfaces) {
            if (!first) {
                output += ", ";
            }
            first = false;
            fmt::format_to(std::back_inserter(output), "{}({})", iface.second, iface.first.value());
        }
        return output;
    }

#if RAV_HAS_BOOST_JSON
    /**
     * @returns A JSON representation of the network interface configuration.
     */
    [[nodiscard]] boost::json::array to_boost_json() const {
        auto a = boost::json::array();

        for (auto& iface : interfaces) {
            a.push_back(boost::json::object {{"rank", iface.first.value()}, {"identifier", iface.second}});
        }

        return a;
    }

    /**
     * Creates a NetworkInterfaceConfig from a JSON object.
     * @param json The json to restore from.
     * @return A newly constructed NetworkInterfaceConfig object.
     */
    [[nodiscard]] static tl::expected<NetworkInterfaceConfig, std::string> from_boost_json(const boost::json::value& json) {
        const auto json_array = json.try_as_array();
        if (json_array.has_error()) {
            return tl::unexpected("Value is not an array");
        }
        NetworkInterfaceConfig config;
        try {
            for (auto& object : *json_array) {
                const auto rank = Rank(object.at("rank").to_number<uint8_t>());
                const auto identifier = object.at("identifier").as_string();
                config.interfaces[rank] = identifier;
            }
        } catch (const std::exception& e) {
            return tl::unexpected(fmt::format("Failed to parse NetworkInterfaceConfig: {}", e.what()));
        }
        return config;
    }
#endif
};

}  // namespace rav
