/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include <boost/asio.hpp>
#include "ravennakit/core/format.hpp"

namespace rav::rtp {

struct Session {
    boost::asio::ip::address connection_address; // TODO: Make v4
    uint16_t rtp_port {};
    uint16_t rtcp_port {};

    void reset() {
        connection_address = boost::asio::ip::address{};
        rtp_port = {};
        rtcp_port = {};
    }

    [[nodiscard]] std::string to_string() const {
        return fmt::format("{}/{}/{}", connection_address.to_string(), rtp_port, rtcp_port);
    }

    [[nodiscard]] bool valid() const {
        return !connection_address.is_unspecified() && rtp_port != 0 && rtcp_port != 0;
    }

    friend auto operator==(const Session& lhs, const Session& rhs) -> bool {
        return std::tie(lhs.connection_address, lhs.rtp_port, lhs.rtcp_port)
            == std::tie(rhs.connection_address, rhs.rtp_port, rhs.rtcp_port);
    }

    friend bool operator!=(const Session& lhs, const Session& rhs) {
        return !(lhs == rhs);
    }
};

}  // namespace rav
