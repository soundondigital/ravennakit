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
#include <boost/beast/http/status.hpp>

namespace rav::nmos {

struct ApiError {
    unsigned code {};
    std::string error {};
    std::string debug {};

    ApiError() = default;

    ApiError(boost::beast::http::status status, std::string error_msg, std::string debug_msg = {}) :
        code(static_cast<decltype(code)>(status)), error(std::move(error_msg)), debug(std::move(debug_msg)) {
        if (debug.empty()) {
            debug = "error: " + error;
        }
    }
};

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const ApiError& value) {
    jv = {{"code", value.code}, {"error", value.error}, {"debug", value.debug}};
}

inline ApiError tag_invoke(const boost::json::value_to_tag<ApiError>&, const boost::json::value& jv) {
    auto obj = jv.as_object();
    ApiError error;
    error.code = static_cast<uint32_t>(obj.at("code").as_int64());
    error.error = obj.at("error").as_string();
    if (const auto v = obj.at("debug").try_as_string()) {
        error.debug = *v;
    }
    return error;
}

}  // namespace rav::nmos
