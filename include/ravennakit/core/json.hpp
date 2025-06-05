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

#ifndef RAV_ENABLE_JSON
    #define RAV_ENABLE_JSON
#endif

#ifdef RAV_ENABLE_JSON
    #define RAV_HAS_NLOHMANN_JSON 1
    #include "expected.hpp"
    #include <nlohmann/json.hpp>
#else
    #define RAV_HAS_NLOHMANN_JSON 0
#endif

#include <boost/json.hpp>

namespace rav {

template<typename T>
boost::system::result<T> parse_json(const std::string_view json_str) {
    boost::system::error_code ec;
    const auto jv = boost::json::parse(json_str, ec);
    if (ec) {
        return ec;
    }
    return boost::json::try_value_to<T>(jv);
}

}  // namespace rav
