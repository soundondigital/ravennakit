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

#include <ravennakit/containers/buffer_view.hpp>
#include <ravennakit/core/util.hpp>

static_assert(std::is_trivially_copyable_v<rav::buffer_view<double>> == true);
static_assert(std::is_trivially_copyable_v<rav::buffer_view<float>> == true);
static_assert(std::is_trivially_copyable_v<rav::buffer_view<int>> == true);
static_assert(std::is_trivially_copyable_v<rav::buffer_view<char>> == true);

TEST_CASE("buffer_view::buffer_view()", "[buffer_view]") {
    SECTION("Test int buffer") {
        int data[] = {1, 2, 3, 4, 5};
        const rav::buffer_view buffer_view(data, rav::util::num_elements_in_array(data));

        REQUIRE(buffer_view.size() == 5);
        REQUIRE(buffer_view.size_bytes() == 5 * sizeof(int));
        REQUIRE(buffer_view.data() == data);
        REQUIRE(buffer_view.empty() == false);
    }

    SECTION("Test char buffer") {
        char data[] = {1, 2, 3, 4, 5};
        const rav::buffer_view buffer_view(data, rav::util::num_elements_in_array(data));

        REQUIRE(buffer_view.size() == 5);
        REQUIRE(buffer_view.size_bytes() == 5 * sizeof(char));
        REQUIRE(buffer_view.data() == data);
        REQUIRE(buffer_view.empty() == false);
    }

    SECTION("Test empty buffer") {
        int data = 5;
        const rav::buffer_view buffer_view(&data, 0);

        REQUIRE(buffer_view.size() == 0);  // NOLINT
        REQUIRE(buffer_view.size_bytes() == 0);
        REQUIRE(buffer_view.data() == &data);
        REQUIRE(buffer_view.empty() == true);
    }

    SECTION("Test invalid buffer") {
        const rav::buffer_view<int> buffer_view(nullptr, 1);

        REQUIRE(buffer_view.size() == 0);  // NOLINT
        REQUIRE(buffer_view.size_bytes() == 0);
        REQUIRE(buffer_view.data() == nullptr);
        REQUIRE(buffer_view.empty() == true);
    }

    SECTION("buffer_view can be copied") {
        int data[] = {1, 2, 3, 4, 5};
        const rav::buffer_view buffer_view(data, rav::util::num_elements_in_array(data));
        const rav::buffer_view buffer_view_copy(buffer_view);

        REQUIRE(buffer_view.data() == buffer_view_copy.data());
        REQUIRE(buffer_view.size() == buffer_view_copy.size());
        REQUIRE(buffer_view.size_bytes() == buffer_view_copy.size_bytes());
        REQUIRE(buffer_view.empty() == buffer_view_copy.empty());
    }
}
