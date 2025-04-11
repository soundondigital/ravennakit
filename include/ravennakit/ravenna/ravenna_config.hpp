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

#include "ravennakit/core/net/interfaces/network_interface.hpp"
#include "ravennakit/core/net/interfaces/network_interface_list.hpp"
#include "ravennakit/core/json.hpp"

namespace rav {

/**
 * Configuration for RAVENNA related classes.
 * This class is used to configure the Ravenna node and its components.
 */
struct RavennaConfig {
    struct NetworkInterfaceConfig {
        enum class Rank {
            primary,
            secondary,
        };

        std::optional<NetworkInterface::Identifier> primary;
        std::optional<NetworkInterface::Identifier> secondary;

        /**
         * @return The first IPv4 address of one of the network interfaces. The address will be empty if the interface
         * is not found or if it has no IPv4 address.
         */
        [[nodiscard]] asio::ip::address_v4 get_ipv4_address(const Rank rank) const {
            if (rank == Rank::primary && primary) {
                if (auto* interface = NetworkInterfaceList::get_system_interfaces().get_interface(*primary)) {
                    return interface->get_first_ipv4_address();
                }
            } else if (rank == Rank::secondary && secondary) {
                if (auto* interface = NetworkInterfaceList::get_system_interfaces().get_interface(*secondary)) {
                    return interface->get_first_ipv4_address();
                }
            }

            return asio::ip::address_v4 {};
        }

        /**
         * @return A string representation of the network interface configuration.
         */
        [[nodiscard]] std::string to_string() const {
            return fmt::format(
                R"(Network interface configuration: primary={}, secondary={})", primary ? *primary : "none",
                secondary ? *secondary : "none"
            );
        }

        /**
         * @returns A JSON representation of the network interface configuration.
         */
        [[nodiscard]] nlohmann::json to_json() const {
            nlohmann::json j;
            j["primary"] = primary.has_value() ? nlohmann::json(*primary) : nlohmann::json {};
            j["secondary"] = secondary.has_value() ? nlohmann::json(*secondary) : nlohmann::json {};
            return j;
        }

        /**
         * Creates a NetworkInterfaceConfig from a JSON object.
         * @param json
         * @return A newly constructed NetworkInterfaceConfig object.
         */
        static tl::expected<NetworkInterfaceConfig, std::string> from_json(const nlohmann::json& json) {
            NetworkInterfaceConfig config;
            try {
                const auto& pri = json.at("primary");
                if (!pri.is_null()) {
                    config.primary = pri.get<NetworkInterface::Identifier>();
                }

                const auto& sec = json.at("secondary");
                if (!sec.is_null()) {
                    config.secondary = sec.get<NetworkInterface::Identifier>();
                }
            } catch (const std::exception& e) {
                return tl::unexpected(fmt::format("Failed to parse NetworkInterfaceConfig: {}", e.what()));
            }
            return config;
        }
    };

    /**
     * @return A string representation of the configuration.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format("RAVENNA Configuration: {}", network_interfaces.to_string());
    }

    /**
     * @returns A JSON representation of the configuration.
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["network_config"] = network_interfaces.to_json();
        return j;
    }

    /**
     * Creates a RavennaConfig from a JSON object.
     * @param json The JSON object to parse.
     * @return A newly constructed RavennaConfig object.
     */
    static tl::expected<RavennaConfig, std::string> from_json(const nlohmann::json& json) {
        try {
            RavennaConfig config;
            auto result = NetworkInterfaceConfig::from_json(json.at("network_config"));
            if (!result) {
                return tl::unexpected(result.error());
            }
            config.network_interfaces = result.value();
            return config;
        } catch (const std::exception& e) {
            return tl::unexpected(fmt::format("Failed to parse RavennaConfig: {}", e.what()));
        }
    }

    NetworkInterfaceConfig network_interfaces;
};

}  // namespace rav
