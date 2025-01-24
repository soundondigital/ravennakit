/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/events/event_emitter.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("event_emitter") {
    SECTION("Subscribing") {
        int times_called = 0;
        rav::event_emitter<std::string> emitter;
        auto slot = emitter.subscribe([&times_called](const std::string& value) {
            REQUIRE(value == "Hello, world!");
            times_called++;
        });
        emitter.emit("Hello, world!");
        REQUIRE(times_called == 1);
    }

    SECTION("Subscribing with 2 arguments") {
        int times_called = 0;
        rav::event_emitter<std::string, int> emitter;
        auto slot = emitter.subscribe([&times_called](const std::string& value, const int& number) {
            REQUIRE(value == "Hello, world!");
            REQUIRE(number == 5);
            times_called++;
        });
        emitter.emit("Hello, world!", 5);
        REQUIRE(times_called == 1);
    }

    SECTION("Subscribing, emitting, unsubscribing, and emitting again") {
        int times_called = 0;
        rav::event_emitter<std::string> emitter;
        {
            auto slot = emitter.subscribe([&times_called](const std::string& value) {
                REQUIRE(value == "Hello, world!");
                times_called++;
            });
            emitter.emit("Hello, world!");
            REQUIRE(times_called == 1);
        }
        emitter.emit("Hello, world!");
        REQUIRE(times_called == 1);
    }

    SECTION("Subscribing without storing the handle should not call the handler") {
        int times_called = 0;
        rav::event_emitter<std::string> emitter;
        // By not keeping the slot, the subscription is removed immediately
        std::ignore = emitter.subscribe([&times_called](const std::string&) {
            times_called++;
        });
        emitter.emit("Hello, world!");
        REQUIRE(times_called == 0);
    }
}
