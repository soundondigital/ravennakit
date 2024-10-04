/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/event_emitter.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/containers/vector_stream.hpp"

namespace {

struct str_event {
    std::string string;
};

struct int_event {
    int number;
};

class publisher final: public rav::event_emitter<publisher, str_event, int_event> {
  public:
    template<class Type>
    void publish_event(const Type& event) {
        emit(event);
    }
};

}  // namespace

TEST_CASE("event_publisher", "[event_publisher]") {
    rav::vector_stream<std::string> events;

    publisher p;
    p.on<str_event>([&events](const str_event& event, publisher&) {
        events.push_back(event.string);
    });
    p.on<int_event>([&events](const int_event& event, publisher&) {
        events.push_back(std::to_string(event.number));
    });

    p.publish_event(str_event {"Hello"});

    REQUIRE(events.read() == "Hello");
    REQUIRE(events.empty());

    p.publish_event(int_event {42});

    REQUIRE(events.read() == "42");
    REQUIRE(events.empty());

    p.reset<str_event>();

    p.publish_event(str_event {"Hello"});
    p.publish_event(int_event {42});

    REQUIRE(events.read() == "42");
    REQUIRE(events.empty());

    p.reset();

    p.publish_event(str_event {"Hello"});
    p.publish_event(int_event {42});

    REQUIRE(events.empty());
}
