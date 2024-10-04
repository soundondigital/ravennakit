/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/dnssd/service_description.hpp"

#include <sstream>

std::string rav::dnssd::service_description::description() const noexcept {
    std::string txtRecordDescription;

    for (auto& kv : txt)
    {
        txtRecordDescription += kv.first;
        txtRecordDescription += "=";
        txtRecordDescription += kv.second;
        txtRecordDescription += ", ";
    }

    std::string addressesDescription;

    for (auto& interface : interfaces)
    {
        addressesDescription += "interface ";
        addressesDescription += std::to_string (interface.first);
        addressesDescription += ": ";

        for (auto& addr : interface.second)
        {
            addressesDescription += addr;
            addressesDescription += ", ";
        }
    }

    std::stringstream output;

    output << "fullname: " << fullname << ", name: " << name << ", type: " << type << ", domain: " << domain
           << ", hostTarget: " << host << ", port: " << port << ", txtRecord: " << txtRecordDescription
           << "addresses: " << addressesDescription;

    return output.str();
}
