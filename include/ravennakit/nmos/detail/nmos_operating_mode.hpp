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

#include "ravennakit/core/json.hpp"

#include <fmt/ostream.h>

namespace rav::nmos {

/**
 * The mode of operation for the NMOS node.
 */
enum class OperationMode {
    /// Discovers registries via mDNS, falling back to p2p if no registry is available.
    mdns_p2p,
    /// Connects to a registry via a manually specified address.
    manual,
    /// The node does not register with a registry and only does peer-to-peer discovery.
    p2p,
};

inline const char* to_string(const OperationMode operation_mode) {
    switch (operation_mode) {
        case OperationMode::mdns_p2p:
            return "mdns_p2p";
        case OperationMode::manual:
            return "manual";
        case OperationMode::p2p:
            return "p2p";
    }
    return "unknown";
}

inline std::optional<OperationMode> operation_mode_from_string(const std::string_view str) {
    if (str == "mdns_p2p") {
        return OperationMode::mdns_p2p;
    }
    if (str == "manual") {
        return OperationMode::manual;
    }
    if (str == "p2p") {
        return OperationMode::p2p;
    }
    return std::nullopt;
}

/// Overload the output stream operator for the Node::Error enum class
inline std::ostream& operator<<(std::ostream& os, const OperationMode operation_mode) {
    os << to_string(operation_mode);
    return os;
}

}  // namespace rav::nmos

/// Make Node::OperationMode printable with fmt
template<>
struct fmt::formatter<rav::nmos::OperationMode>: ostream_formatter {};
