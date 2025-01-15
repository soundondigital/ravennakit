#pragma once

#include <map>
#include <set>
#include <string>
#include <cstdint>

namespace rav::dnssd
{

/// Simple typedef for representing a TXT record.
using txt_record = std::map<std::string, std::string>;

/**
 * A struct containing data which represents a service on the network.
 */
struct service_description
{
    /// The full service domain name.
    std::string fullname;

    /// The name of the service.
    std::string name;

    /// The type of the service (ie. _http._tcp.).
    std::string reg_type;

    /// The domain of the service (local.).
    std::string domain;

    /// The host target of the service (name.local.).
    std::string host_target;

    /// The port of the service (in native endian).
    uint16_t port{};

    /// The TXT record of the service, represented as a map of keys and values.
    txt_record txt;

    /// The resolved addresses of this service.
    std::map<uint32_t, std::set<std::string>> interfaces; // interfaceIndex, addresses

    /// Returns a description of this struct, which might be handy for debugging or logging purposes.
    [[nodiscard]] std::string description() const noexcept;
};

} // namespace dnssd
