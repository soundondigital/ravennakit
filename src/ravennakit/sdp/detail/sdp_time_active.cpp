/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_time_active.hpp"

#include "ravennakit/core/string_parser.hpp"

tl::expected<rav::sdp::TimeActiveField, std::string> rav::sdp::parse_time_active(std::string_view line) {
    StringParser parser(line);

    if (!parser.skip("t=")) {
        return tl::unexpected("time: expecting 't='");
    }

    TimeActiveField time;

    if (const auto start_time = parser.read_int<int64_t>()) {
        time.start_time = *start_time;
    } else {
        return tl::unexpected("time: failed to parse start time as integer");
    }

    if (!parser.skip(' ')) {
        return tl::unexpected("time: expecting space after start time");
    }

    if (const auto stop_time = parser.read_int<int64_t>()) {
        time.stop_time = *stop_time;
    } else {
        return tl::unexpected("time: failed to parse stop time as integer");
    }

    return time;
}

std::string rav::sdp::to_string(const TimeActiveField& time_active_field) {
    return fmt::format("t={} {}", time_active_field.start_time, time_active_field.stop_time);
}

tl::expected<void, std::string> rav::sdp::validate(const TimeActiveField& time_active_field) {
    if (time_active_field.start_time < 0) {
        return tl::unexpected("time: start time must be greater than or equal to 0");
    }
    if (time_active_field.stop_time < 0) {
        return tl::unexpected("time: stop time must be greater than or equal to 0");
    }
    return {};
}
