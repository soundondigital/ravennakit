/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_source_filter.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("source_filter", "[source_filter]") {
    SECTION("Example 1") {
        auto result = rav::sdp::source_filter::parse_new(" incl IN IP4 232.3.4.5 192.0.2.10");
        REQUIRE(result.is_ok());
        auto filter = result.move_ok();
        REQUIRE(filter.mode() == rav::sdp::filter_mode::include);
        REQUIRE(filter.network_type() == rav::sdp::netw_type::internet);
        REQUIRE(filter.address_type() == rav::sdp::addr_type::ipv4);
        REQUIRE(filter.dest_address() == "232.3.4.5");
        const auto& src_list = filter.src_list();
        REQUIRE(src_list.size() == 1);
        REQUIRE(src_list.front() == "192.0.2.10");
    }

    SECTION("Example 2") {
        auto result = rav::sdp::source_filter::parse_new(" excl IN IP4 192.0.2.11 192.0.2.10");
        REQUIRE(result.is_ok());
        auto filter = result.move_ok();
        REQUIRE(filter.mode() == rav::sdp::filter_mode::exclude);
        REQUIRE(filter.network_type() == rav::sdp::netw_type::internet);
        REQUIRE(filter.address_type() == rav::sdp::addr_type::ipv4);
        REQUIRE(filter.dest_address() == "192.0.2.11");
        const auto& src_list = filter.src_list();
        REQUIRE(src_list.size() == 1);
        REQUIRE(src_list.front() == "192.0.2.10");
    }

    SECTION("Example 3") {
        auto result = rav::sdp::source_filter::parse_new(" incl IN IP4 * 192.0.2.10");
        REQUIRE(result.is_ok());
        auto filter = result.move_ok();
        REQUIRE(filter.mode() == rav::sdp::filter_mode::include);
        REQUIRE(filter.network_type() == rav::sdp::netw_type::internet);
        REQUIRE(filter.address_type() == rav::sdp::addr_type::ipv4);
        REQUIRE(filter.dest_address() == "*");
        const auto& src_list = filter.src_list();
        REQUIRE(src_list.size() == 1);
        REQUIRE(src_list.front() == "192.0.2.10");
    }

    SECTION("Example 4") {
        auto result = rav::sdp::source_filter::parse_new(" incl IN IP6 FF0E::11A 2001:DB8:1:2:240:96FF:FE25:8EC9");
        REQUIRE(result.is_ok());
        auto filter = result.move_ok();
        REQUIRE(filter.mode() == rav::sdp::filter_mode::include);
        REQUIRE(filter.network_type() == rav::sdp::netw_type::internet);
        REQUIRE(filter.address_type() == rav::sdp::addr_type::ipv6);
        REQUIRE(filter.dest_address() == "FF0E::11A");
        const auto& src_list = filter.src_list();
        REQUIRE(src_list.size() == 1);
        REQUIRE(src_list.front() == "2001:DB8:1:2:240:96FF:FE25:8EC9");
    }

    SECTION("Example 5") {
        auto result = rav::sdp::source_filter::parse_new(" incl IN * dst-1.example.com src-1.example.com src-2.example.com");
        REQUIRE(result.is_ok());
        auto filter = result.move_ok();
        REQUIRE(filter.mode() == rav::sdp::filter_mode::include);
        REQUIRE(filter.network_type() == rav::sdp::netw_type::internet);
        REQUIRE(filter.address_type() == rav::sdp::addr_type::both);
        REQUIRE(filter.dest_address() == "dst-1.example.com");
        const auto& src_list = filter.src_list();
        REQUIRE(src_list.size() == 2);
        REQUIRE(src_list.at(0) == "src-1.example.com");
        REQUIRE(src_list.at(1) == "src-2.example.com");
    }
}
