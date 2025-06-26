/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/sdp/detail/sdp_media_clock_source.hpp"

const char* rav::sdp::to_string(const MediaClockSource::ClockMode mode) {
    switch (mode) {
        case MediaClockSource::ClockMode::undefined:
            return "undefined";
        case MediaClockSource::ClockMode::direct:
        default:
            return "direct";
    }
}

std::string rav::sdp::to_string(const MediaClockSource& mode) {
    auto txt = fmt::format("a={}:{}", MediaClockSource::k_attribute_name, to_string(mode.mode));
    if (mode.offset) {
        fmt::format_to(std::back_inserter(txt), "={}", *mode.offset);
    }
    if (mode.rate) {
        fmt::format_to(std::back_inserter(txt), " rate={}/{}", mode.rate->numerator, mode.rate->denominator);
    }
    return txt;
}

tl::expected<void, std::string> rav::sdp::validate(const MediaClockSource& clock_source) {
    if (clock_source.mode == MediaClockSource::ClockMode::undefined) {
        return tl::unexpected("media_clock: mode is undefined");
    }
    return {};
}

tl::expected<rav::sdp::MediaClockSource, std::string> rav::sdp::parse_media_clock_source(std::string_view line) {
    StringParser parser(line);

    MediaClockSource clock;

    if (const auto mode_part = parser.split(' ')) {
        StringParser mode_parser(*mode_part);

        if (const auto mode = mode_parser.split('=')) {
            if (mode == "direct") {
                clock.mode = MediaClockSource::ClockMode::direct;
            } else {
                RAV_WARNING("Unsupported media clock mode: {}", *mode);
                return tl::unexpected("media_clock: unsupported media clock mode");
            }
        } else {
            return tl::unexpected("media_clock: invalid media clock mode");
        }

        if (!mode_parser.exhausted()) {
            if (const auto offset = mode_parser.read_int<int64_t>()) {
                clock.offset = *offset;
            } else {
                return tl::unexpected("media_clock: invalid offset");
            }
        }
    }

    if (parser.exhausted()) {
        return clock;
    }

    if (const auto rate = parser.split('=')) {
        if (rate == "rate") {
            const auto numerator = parser.read_int<int32_t>();
            if (!numerator) {
                return tl::unexpected("media_clock: invalid rate numerator");
            }
            if (!parser.skip('/')) {
                return tl::unexpected("media_clock: invalid rate denominator");
            }
            const auto denominator = parser.read_int<int32_t>();
            if (!denominator) {
                return tl::unexpected("media_clock: invalid rate denominator");
            }
            clock.rate = Fraction<int32_t> {*numerator, *denominator};
        } else {
            return tl::unexpected("media_clock: unexpected token");
        }
    } else {
        return tl::unexpected("media_clock: expecting rate");
    }

    return clock;
}
