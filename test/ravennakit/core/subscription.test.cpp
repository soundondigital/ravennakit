/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <catch2/catch_all.hpp>

#include "ravennakit/core/subscription.hpp"

TEST_CASE("subscription") {
    SECTION("Basic functionality") {
        int count = 0;
        {
            rav::subscription subscription([&count] {
                count++;
            });
            REQUIRE(count == 0);
        }
        REQUIRE(count == 1);
    }

    SECTION("Move construct") {
        int count = 0;
        {
            rav::subscription subscription([&count] {
                count++;
            });
            rav::subscription subscription2(std::move(subscription));
            REQUIRE(count == 0);
        }
        REQUIRE(count == 1);
    }

    SECTION("Move assign") {
        int count_a = 0;
        int count_b = 0;
        {
            rav::subscription subscription_a([&count_a] {
                count_a++;
            });

            rav::subscription subscription_b([&count_b] {
                count_b++;
            });

            REQUIRE(count_a == 0);
            REQUIRE(count_b == 0);

            subscription_a = std::move(subscription_b);

            REQUIRE(count_a == 1);
            REQUIRE(count_b == 0);
        }
        REQUIRE(count_a == 1);
        REQUIRE(count_b == 1);
    }

    SECTION("Assign new callback") {
        int count = 0;
        {
            rav::subscription subscription([&count] {
                count++;
            });
            REQUIRE(count == 0);

            subscription = [&count] {
                count++;
            };

            REQUIRE(count == 1);
        }
        REQUIRE(count == 2);
    }

    SECTION("Release subscription") {
        int count = 0;
        {
            rav::subscription subscription([&count] {
                count++;
            });
            subscription.release();
            REQUIRE(count == 0);
        }
        REQUIRE(count == 0);
    }

    SECTION("Reset subscription") {
        int count = 0;
        {
            rav::subscription subscription([&count] {
                count++;
            });
            subscription.reset();
            REQUIRE(count == 1);
        }
        REQUIRE(count == 1);
    }

    SECTION("Operator bool") {
        rav::subscription subscription([] {});
        REQUIRE(subscription);
        rav::subscription empty_subscription;
        REQUIRE_FALSE(empty_subscription);
    }
}
