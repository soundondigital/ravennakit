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

#include "datasets/ptp_port_ds.hpp"
#include "ravennakit/rtp/detail/udp_sender_receiver.hpp"
#include "types/ptp_port_identity.hpp"

#include <optional>
#include <vector>

namespace rav {

class ptp_port {
  public:
    ptp_port(asio::io_context& io_context, const asio::ip::address& interface_address, ptp_port_identity port_identity);

  private:
    ptp_port_ds port_ds_;
    udp_sender_receiver event_socket_;
    udp_sender_receiver general_socket_;
    std::vector<subscription> subscriptions_;

    struct foreign_master_entry {
        ptp_port_identity foreign_master_port_identity;
        size_t foreign_master_announce_messages;
        std::optional<char> most_recent_announce_message;  // Char is a placeholder, update with ptp_announce_message
    };

    std::vector<foreign_master_entry> foreign_master_list_;  // Min capacity: 5
    // Foreign
};

}  // namespace rav
