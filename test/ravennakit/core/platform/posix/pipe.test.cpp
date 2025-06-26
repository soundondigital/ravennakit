/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/platform/posix/pipe.hpp"
#include "catch2/catch_all.hpp"

#if RAV_POSIX

TEST_CASE("rav::posix::Pipe") {
    SECTION("Test default state") {
        rav::posix::Pipe pipe;
        REQUIRE(pipe.read_fd() >= 3);
        REQUIRE(pipe.read_fd() >= 3);
    }

    SECTION("Read and write something") {
        uint64_t in = 0x1234567890abcdef;
        uint64_t out = 0;
        rav::posix::Pipe pipe;
        REQUIRE(pipe.write(&in, sizeof(in)) == sizeof(in));
        REQUIRE(pipe.read(&out, sizeof(out)) == sizeof(out));
        REQUIRE(in == out);
    }

    SECTION("Read a bunch of something") {
        constexpr uint64_t num_elements = 1000; // If this is too large, the test will hang
        rav::posix::Pipe pipe;

        // Write a bunch of data
        for (uint64_t i = 0; i < num_elements; ++i) {
            uint64_t in = i + 0xffff;
            REQUIRE(pipe.write(&in, sizeof(in)) == sizeof(in));
        }

        // Read a bunch of data
        for (uint64_t i = 0; i < num_elements; ++i) {
            uint64_t in = 0;
            REQUIRE(pipe.read(&in, sizeof(in)) == sizeof(in));
            REQUIRE(in == i + 0xffff);
        }
    }
}

#endif
