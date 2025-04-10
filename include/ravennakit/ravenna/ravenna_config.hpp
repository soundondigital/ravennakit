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
#include "ravennakit/core/util/rank.hpp"

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
        asio::ip::address_v4 get_ipv4_address(const Rank rank) const {
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
    };

    /**
     * @return A string representation of the configuration.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format("RAVENNA Configuration: {}", network_interfaces.to_string());
    }

    NetworkInterfaceConfig network_interfaces;
};

}  // namespace rav
