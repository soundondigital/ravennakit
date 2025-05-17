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

#include "ravennakit/nmos/detail/nmos_version.hpp"

#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <map>

namespace rav::nmos {

struct Resource {
    /// Globally unique identifier for the resource
    boost::uuids::uuid id;

    /// String formatted TAI timestamp (<seconds>:<nanoseconds>) indicating precisely when an attribute of the resource
    /// last changed
    Version version;

    /// Freeform string label for the resource
    std::string label;

    /// Detailed description of the resource
    std::string description;

    /// Key value set of freeform string tags to aid in filtering resources. Values should be represented as an array of
    /// strings. Can be empty.
    std::map<std::string, std::vector<std::string>> tags;
};

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Resource& resource) {
    jv = {
        {"id", boost::uuids::to_string(resource.id)},
        {"version", resource.version.to_string()},
        {"label", resource.label},
        {"description", resource.description},
        {"tags", boost::json::value_from(resource.tags)},
    };
}

}  // namespace rav::nmos
