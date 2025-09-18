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

#include <boost/uuid.hpp>
#include <boost/json.hpp>

inline boost::json::value json_value_from_uuid(const boost::uuids::uuid& uuid) {
    return {boost::uuids::to_string(uuid)};
}

inline boost::json::value json_value_from_uuid(const std::optional<boost::uuids::uuid>& uuid) {
    if (uuid.has_value()) {
        return json_value_from_uuid(*uuid);
    }
    return {};
}

inline std::optional<boost::uuids::uuid> uuid_from_json(const boost::json::value& json) {
    if (!json.is_string()) {
        return std::nullopt;
    }
    const auto str = json.as_string();
    if (str.empty()) {
        return std::nullopt;
    }
    return boost::uuids::string_generator()(str.c_str());
}
