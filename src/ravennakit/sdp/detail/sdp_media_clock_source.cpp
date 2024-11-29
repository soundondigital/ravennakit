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

rav::sdp::media_clock_source::media_clock_source(
    const clock_mode mode, const std::optional<int64_t> offset, const std::optional<fraction<int32_t>> rate
) :
    mode_(mode), offset_(offset), rate_(rate) {}

rav::sdp::media_clock_source::parse_result<rav::sdp::media_clock_source>
rav::sdp::media_clock_source::parse_new(const std::string_view line) {
    string_parser parser(line);

    media_clock_source clock;

    if (const auto mode_part = parser.split(' ')) {
        string_parser mode_parser(*mode_part);

        if (const auto mode = mode_parser.split('=')) {
            if (mode == "direct") {
                clock.mode_ = clock_mode::direct;
            } else {
                RAV_WARNING("Unsupported media clock mode: {}", *mode);
                return parse_result<media_clock_source>::err("media_clock: unsupported media clock mode");
            }
        } else {
            return parse_result<media_clock_source>::err("media_clock: invalid media clock mode");
        }

        if (!mode_parser.exhausted()) {
            if (const auto offset = mode_parser.read_int<int64_t>()) {
                clock.offset_ = *offset;
            } else {
                return parse_result<media_clock_source>::err("media_clock: invalid offset");
            }
        }
    }

    if (parser.exhausted()) {
        return parse_result<media_clock_source>::ok(clock);
    }

    if (const auto rate = parser.split('=')) {
        if (rate == "rate") {
            const auto numerator = parser.read_int<int32_t>();
            if (!numerator) {
                return parse_result<media_clock_source>::err("media_clock: invalid rate numerator");
            }
            if (!parser.skip('/')) {
                return parse_result<media_clock_source>::err("media_clock: invalid rate denominator");
            }
            const auto denominator = parser.read_int<int32_t>();
            if (!denominator) {
                return parse_result<media_clock_source>::err("media_clock: invalid rate denominator");
            }
            clock.rate_ = fraction<int32_t> {*numerator, *denominator};
        } else {
            return parse_result<media_clock_source>::err("media_clock: unexpected token");
        }
    } else {
        return parse_result<media_clock_source>::err("media_clock: expecting rate");
    }

    return parse_result<media_clock_source>::ok(clock);
}

rav::sdp::media_clock_source::clock_mode rav::sdp::media_clock_source::mode() const {
    return mode_;
}

std::optional<int64_t> rav::sdp::media_clock_source::offset() const {
    return offset_;
}

const std::optional<rav::fraction<int>>& rav::sdp::media_clock_source::rate() const {
    return rate_;
}

tl::expected<void, std::string> rav::sdp::media_clock_source::validate() const {
    if (mode_ == clock_mode::undefined) {
        return tl::unexpected("media_clock: mode is undefined");
    }
    return {};
}

tl::expected<std::string, std::string> rav::sdp::media_clock_source::to_string() const {
    auto validated = validate();
    if (!validated) {
        return tl::unexpected(validated.error());
    }
    auto txt = fmt::format("a={}:{}", k_attribute_name, to_string(mode_));
    if (offset_) {
        fmt::format_to(std::back_inserter(txt), "={}", *offset_);
    }
    if (rate_) {
        fmt::format_to(std::back_inserter(txt), " rate={}/{}", rate_->numerator, rate_->denominator);
    }
    return txt;
}

std::string rav::sdp::media_clock_source::to_string(const clock_mode mode) {
    switch (mode) {
        case clock_mode::undefined:
            return "undefined";
        case clock_mode::direct:
        default:
            return "direct";
    }
}
