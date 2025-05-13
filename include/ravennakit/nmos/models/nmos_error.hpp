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

#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <boost/json/value_from.hpp>

#include <string>

namespace rav::nmos {

struct Error {
    unsigned code {};
    std::string error {};
    std::string debug {};
};

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Error& value) {
    jv = {{"code", value.code}, {"error", value.error}, {"debug", value.debug}};
}

inline Error tag_invoke(const boost::json::value_to_tag<Error>&, const boost::json::value& jv) {
    auto obj = jv.as_object();
    Error error;
    error.code = static_cast<uint32_t>(obj.at("code").as_int64());
    error.error = obj.at("error").as_string();
    if (const auto v = obj.at("debug").try_as_string()) {
        error.debug = *v;
    }
    return error;
}

}  // namespace rav::nmos
