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

#include <chrono>

namespace rav {

/**
 * A list of network interfaces with some convenience functions to operate on them.
 */
class NetworkInterfaceList {
  public:
    NetworkInterfaceList() = default;

    /**
     * Constructor that takes a vector of network interfaces.
     * @param interfaces The vector of network interfaces.
     */
    explicit NetworkInterfaceList(std::vector<NetworkInterface> interfaces);

    /**
     * Gets a network interface by the given identifier.
     * @param identifier The string to search for.
     * @return The network interface if found, otherwise nullptr.
     */
    [[nodiscard]] const NetworkInterface* get_interface(const NetworkInterface::Identifier& identifier) const;

    /**
     * Finds a network interface by the given string. The string can be the identifier, display name, description, MAC
     * or an ip address. It's meant as convenience function for the user. The search is case-insensitive.
     * @param search_string The string to search for.
     * @return The network interface if found, otherwise nullptr.
     */
    [[nodiscard]] const NetworkInterface* find_by_string(const std::string& search_string) const;

    /**
     * Finds a network interface by the given address.
     * @param address The address to search for.
     * @return The network interface if found, otherwise nullptr.
     */
    [[nodiscard]] const NetworkInterface* find_by_address(const boost::asio::ip::address& address) const;

    /**
     * Finds a network interface by the given type.
     * @param type The type of the interface to search for.
     * @return A pointer to a network interface if found, otherwise nullptr.
     */
    [[nodiscard]] const NetworkInterface* find_by_type(NetworkInterface::Type type) const;

    /**
     * @returns The list of network interfaces.
     */
    [[nodiscard]] const std::vector<NetworkInterface>& get_interfaces() const;

    /**
     * @return The list of network interface identifiers.
     */
    [[nodiscard]] std::vector<NetworkInterface::Identifier> get_interface_identifiers() const;

    /**
     * Retrieves the list of network interfaces on the system. This is a static function that returns a singleton
     * instance of the NetworkInterfaceList. This will return an updated list when the ttl has expired or if the
     * force_refresh parameter is set to true.
     * @throws std::runtime_error if the list cannot be retrieved.
     * @param force_refresh If true, the list is refreshed even if the ttl has not expired.
     * @return The list of network interfaces on the system.
     */
    static const NetworkInterfaceList& get_system_interfaces(bool force_refresh = false);

  private:
    static constexpr auto k_ttl = std::chrono::seconds(5); // TTL for the cache
    std::vector<NetworkInterface> interfaces_;

    /**
     * Refreshes the list with interfaces on the system.
     */
    void repopulate_with_system_interfaces();
};

}  // namespace rav
