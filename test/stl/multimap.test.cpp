/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include <catch2/catch_all.hpp>

#include <map>

TEST_CASE("std::multimap") {
    SECTION("Test order") {
        std::multimap<int, int> map;
        map.insert({1,4});
        map.insert({1,3});
        map.insert({0,2});
        map.insert({-1,1});

        std::vector<std::pair<int, int>> kvs;
        for (const auto& [k, v] : map) {
            kvs.emplace_back(k, v);
        }

        REQUIRE(kvs[0] == std::pair(-1,1));
        REQUIRE(kvs[1] == std::pair(0,2));
        REQUIRE(kvs[2] == std::pair(1,4));
        REQUIRE(kvs[3] == std::pair(1,3));
    }
}
