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

#include "ptp_error.hpp"
#include "ptp_port.hpp"
#include "datasets/ptp_current_ds.hpp"
#include "datasets/ptp_default_ds.hpp"
#include "datasets/ptp_parent_ds.hpp"
#include "ravennakit/core/net/interfaces/network_interface_list.hpp"

#include <asio/ip/address.hpp>
#include <tl/expected.hpp>

namespace rav {

class ptp_instance {
  public:
    explicit ptp_instance(asio::io_context& io_context);

    /**
     * Adds a port to the PTP instance. The port will be used to send and receive PTP messages. The clock identity of
     * the PTP instance will be determined by the first port added, based on its MAC address.
     * @param interface_address The address of the interface to bind the port to. The network interface must have a MAC address
     * and support multicast.
     */
    tl::expected<void, ptp_error> add_port(const asio::ip::address& interface_address);

  private:
    asio::io_context& io_context_;
    ptp_default_ds default_ds_;
    ptp_current_ds current_ds_;
    ptp_parent_ds parent_ds_;
    std::vector<std::unique_ptr<ptp_port>> ports_;
    network_interface_list network_interfaces_;

    // Local PTP clock
    // Local clock

    [[nodiscard]] uint16_t get_next_available_port_number() const;
};

}  // namespace rav
