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

#include "network_interface.hpp"

namespace rav {

/**
 * A list of network interfaces on the system.
 */
class network_interface_list {
  public:
    network_interface_list();

    /**
     * Refreshes the list with interfaces on the system.
     */
    void refresh();

    /**
     * Finds a network interface by the given string. The string can be the identifier, display name, description, MAC
     * or an ip address. It's meant as convenience function for the user.
     * @param search_string The string to search for.
     * @return The network interface if found, otherwise nullptr.
     */
    [[nodiscard]] const network_interface* find_by_string(const std::string& search_string) const;

    /**
     * Finds a network interface by the given address.
     * @param address The address to search for.
     * @return The network interface if found, otherwise nullptr.
     */
    [[nodiscard]] const network_interface* find_by_address(const asio::ip::address& address) const;

    /**
     * @returns The list of network interfaces.
     */
    [[nodiscard]] const std::vector<network_interface>& interfaces() const;

  private:
    std::vector<network_interface> interfaces_;
};

}  // namespace rav
