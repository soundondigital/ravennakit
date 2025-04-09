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
#include "ravennakit/core/util/rank.hpp"

namespace rav {

/**
 * Configuration for RAVENNA related classes.
 * This class is used to configure the Ravenna node and its components.
 */
struct RavennaConfig {
    struct NetworkInterfaceConfig {
        std::optional<NetworkInterface::Identifier> primary;
        std::optional<NetworkInterface::Identifier> secondary;

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
