/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/nmos/models/nmos_error.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("nmos::Error") {
    SECTION("To json") {
        rav::nmos::Error error;
        error.code = 404;
        error.error = "Not found";
        error.debug = "The requested resource was not found";

        boost::json::value v = boost::json::value_from(error);

        auto j = boost::json::serialize(v);
        REQUIRE(j == R"({"code":404,"error":"Not found","debug":"The requested resource was not found"})");
    }

    SECTION("From json") {
        SECTION("All fields are present") {
            auto error = boost::json::value_to<rav::nmos::Error>(
                boost::json::parse(R"({"code":404,"error":"Not found","debug":"The requested resource was not found"})")
            );

            REQUIRE(error.code == 404);
            REQUIRE(error.error == "Not found");
            REQUIRE(error.debug == "The requested resource was not found");
        }

        SECTION("Debug is null") {
            auto error = boost::json::value_to<rav::nmos::Error>(
                boost::json::parse(R"({"code":404,"error":"Not found","debug":null})")
            );
            REQUIRE(error.code == 404);
            REQUIRE(error.error == "Not found");
            REQUIRE(error.debug.empty());
        }
    }
}
