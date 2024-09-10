/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/rollback.hpp"

#include "catch2/catch_all.hpp"

TEST_CASE("rollback::rollback()", "[rollback]") {
    int count = 0;
    SECTION("Rollback with initial function") {
        {
            rav::rollback rollback([&count]() {
                count++;
            });
        }
        REQUIRE(count == 1);
    }

    SECTION("Rollback with initial and added function") {
        {
            rav::rollback rollback([&count]() {
                count++;
            });
            rollback.add([&count]() {
                count++;
            });
        }
        REQUIRE(count == 2);
    }

    SECTION("Rollback won't happen when cancelled") {
        {
            rav::rollback rollback([&count]() {
                count++;
            });
            rollback.add([&count]() {
                count++;
            });
            rollback.commit();
        }
        REQUIRE(count == 0);
    }
}
