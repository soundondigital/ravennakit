/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/json.hpp"
#include "ravennakit/nmos/models/nmos_api_error.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("nmos::ApiError") {
    SECTION("To json") {
        rav::nmos::ApiError error;
        error.code = 404;
        error.error = "Not found";
        error.debug = "The requested resource was not found";

        boost::json::value v = boost::json::value_from(error);

        auto j = boost::json::serialize(v);
        REQUIRE(j == R"({"code":404,"error":"Not found","debug":"The requested resource was not found"})");
    }

    SECTION("From json") {
        SECTION("All fields are present") {
            auto error = boost::json::value_to<rav::nmos::ApiError>(
                boost::json::parse(R"({"code":404,"error":"Not found","debug":"The requested resource was not found"})")
            );

            REQUIRE(error.code == 404);
            REQUIRE(error.error == "Not found");
            REQUIRE(error.debug == "The requested resource was not found");
        }

        SECTION("Debug is null") {
            auto error = boost::json::value_to<rav::nmos::ApiError>(
                boost::json::parse(R"({"code":404,"error":"Not found","debug":null})")
            );
            REQUIRE(error.code == 404);
            REQUIRE(error.error == "Not found");
            REQUIRE(error.debug.empty());
        }
    }

    SECTION("Parse") {
        SECTION("Valid JSON") {
            auto result = rav::parse_json<rav::nmos::ApiError>(
                R"({"code":400,"error":"Bad Request; request for registration with version 1:0 conflicts with the existing registration with version 1:0","debug":null})"
            );
            REQUIRE(result.has_value());
            REQUIRE(result->code == 400);
            REQUIRE(
                result->error
                == "Bad Request; request for registration with version 1:0 conflicts with the existing registration with version 1:0"
            );
            REQUIRE(result->debug.empty());
        }

        SECTION("Invalid JSON") {
            auto result = rav::parse_json<rav::nmos::ApiError>(R"({"code":404,"error":"Not found",})");
            REQUIRE(result.has_error());
        }
    }
}
