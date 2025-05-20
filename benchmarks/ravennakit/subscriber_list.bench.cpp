/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/subscriber_list.hpp"

#include <catch2/catch_all.hpp>
#include <nanobench.h>

namespace {

class Subscriber {
  public:
    virtual ~Subscriber() = default;
    virtual void on_event(const std::string& event) = 0;
};

class ConcreteSubscriber: public Subscriber {
  public:
    std::string last_value;
    void on_event(const std::string& event) override {
        last_value = event;
        ankerl::nanobench::doNotOptimizeAway(event);
    }
};

}  // namespace

TEST_CASE("SubscriberList Benchmark") {
    ankerl::nanobench::Bench b;
    b.title("SubscriberList Benchmark")
        .warmup(100)
        .relative(false)
        .minEpochIterations(1'000'000)
        .performanceCounters(true);

    rav::SubscriberList<Subscriber> subscriber_list;
    ConcreteSubscriber subscriber;
    std::ignore = subscriber_list.add(&subscriber);

    int i_int = 0;
    b.run("Using foreach", [&] {
        subscriber_list.foreach([&](Subscriber* s) {
            s->on_event(std::to_string(i_int++));
        });
    });

    b.run("Using range based for", [&] {
        for (auto* s: subscriber_list) {
            s->on_event(std::to_string(i_int++));
        }
    });
}
