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
#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/core/net/asio/asio_helpers.hpp"
#include "ravennakit/rtp/detail/rtp_audio_receiver.hpp"
#include "ravennakit/rtp/detail/rtp_audio_sender.hpp"

#include <vector>

namespace rav {

class NetworkInterfaceConfig {
  public:
    std::vector<NetworkInterface::Identifier> interfaces;

    friend bool operator==(const NetworkInterfaceConfig& lhs, const NetworkInterfaceConfig& rhs) {
        return lhs.interfaces == rhs.interfaces;
    }

    friend bool operator!=(const NetworkInterfaceConfig& lhs, const NetworkInterfaceConfig& rhs) {
        return lhs.interfaces != rhs.interfaces;
    }

    /**
     * Sets a network interface by its rank.
     * @param rank The rank (index) of the network interface.
     * @param identifier The identifier of the network interface. Pas an empty identifier to clear the interface.
     */
    void set_interface(const size_t rank, const NetworkInterface::Identifier& identifier) {
        if (interfaces.size() < rank + 1) {
            interfaces.resize(rank + 1);
        }
        interfaces[rank] = identifier;
    }

    /**
     * Gets a network interface by its rank.
     * @param rank The rank (index) of the network interface.
     * @return A pointer to the network interface identifier if found, or nullptr if not found.
     */
    [[nodiscard]] const NetworkInterface::Identifier* get_interface_for_rank(const size_t rank) const {
        if (interfaces.size() < rank + 1) {
            return nullptr;
        }
        return &interfaces[rank];
    }

    /**
     * @return The first IPv4 address of one of the network interfaces. The address will be unspecified if the
     * interface is not found or if it has no IPv4 address.
     */
    [[nodiscard]] ip_address_v4 get_interface_ipv4_address(const size_t rank) const {
        auto* id = get_interface_for_rank(rank);
        if (id == nullptr) {
            return {};
        }
        if (auto* interface = NetworkInterfaceList::get_system_interfaces().get_interface(*id)) {
            return interface->get_first_ipv4_address();
        }
        return {};
    }

    /**
     * @return A vector of all network interfaces and their first IPv4 address. The address will be unspecified if the
     * interface has no IPv4 address or is not set.
     */
    [[nodiscard]] std::vector<ip_address_v4> get_interface_ipv4_addresses() const {
        std::vector<ip_address_v4> addresses;
        for (const auto& id : interfaces) {
            auto& it = addresses.emplace_back();
            if (auto* interface = NetworkInterfaceList::get_system_interfaces().get_interface(id)) {
                it = interface->get_first_ipv4_address();
            }
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
        for (size_t i = 0; i < interfaces.size(); ++i) {
            if (!first) {
                output += ", ";
            }
            first = false;
            fmt::format_to(std::back_inserter(output), "{}({})", interfaces[i], i);
        }
        return output;
    }

    /**
     * @tparam N The size of the array.
     * @return An array of addresses of the interfaces, ordered by rank.
     */
    template<size_t N>
    std::array<ip_address_v4, N> get_array_of_interface_addresses() const {
        std::array<ip_address_v4, N> addresses {};
        auto addresses_vector = get_interface_ipv4_addresses();

        for (size_t i = 0; i < std::min(addresses.size(), addresses_vector.size()); ++i) {
            addresses[i] = std::move(addresses_vector[i]);
        }

        return addresses;
    }

#if RAV_HAS_BOOST_JSON
    /**
     * @returns A JSON representation of the network interface configuration.
     */
    [[nodiscard]] boost::json::array to_boost_json() const {
        auto a = boost::json::array();

        for (size_t i = 0; i < interfaces.size(); ++i) {
            a.push_back(boost::json::object {{"rank", i}, {"identifier", interfaces[i]}});
        }

        return a;
    }

    /**
     * Creates a NetworkInterfaceConfig from a JSON object.
     * @param json The json to restore from.
     * @return A newly constructed NetworkInterfaceConfig object.
     */
    [[nodiscard]] static tl::expected<NetworkInterfaceConfig, std::string>
    from_boost_json(const boost::json::value& json) {
        const auto json_array = json.try_as_array();
        if (json_array.has_error()) {
            return tl::unexpected("Value is not an array");
        }
        NetworkInterfaceConfig config;
        try {
            for (auto& object : *json_array) {
                const auto rank = object.at("rank").to_number<size_t>();
                const auto identifier = std::string(object.at("identifier").as_string());
                config.set_interface(rank, identifier);
            }
        } catch (const std::exception& e) {
            return tl::unexpected(fmt::format("Failed to parse NetworkInterfaceConfig: {}", e.what()));
        }
        return config;
    }
#endif
};

inline std::optional<NetworkInterfaceConfig>
parse_network_interface_config_from_string(const std::string& input, const char delimiter = ',') {
    StringParser parser(input);
    NetworkInterfaceConfig config;
    size_t rank {};
    for (auto i = 0; i < 10; ++i) {
        auto section = parser.split(delimiter);
        if (!section) {
            return config;  // Exhausted
        }
        auto* iface = NetworkInterfaceList::get_system_interfaces().find_by_string(*section);
        if (!iface) {
            return std::nullopt;
        }
        config.set_interface(rank++, iface->get_identifier());
    }
    RAV_ASSERT_FALSE("Loop upper bound reached");
    return config;
}

}  // namespace rav
