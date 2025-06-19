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

rav::sdp::MediaClockSource::MediaClockSource(
    const ClockMode mode, const std::optional<int64_t> offset, const std::optional<Fraction<int32_t>> rate
) :
    mode_(mode), offset_(offset), rate_(rate) {}

tl::expected<rav::sdp::MediaClockSource, std::string>
rav::sdp::MediaClockSource::parse_new(const std::string_view line) {
    StringParser parser(line);

    MediaClockSource clock;

    if (const auto mode_part = parser.split(' ')) {
        StringParser mode_parser(*mode_part);

        if (const auto mode = mode_parser.split('=')) {
            if (mode == "direct") {
                clock.mode_ = ClockMode::direct;
            } else {
                RAV_WARNING("Unsupported media clock mode: {}", *mode);
                return tl::unexpected("media_clock: unsupported media clock mode");
            }
        } else {
            return tl::unexpected("media_clock: invalid media clock mode");
        }

        if (!mode_parser.exhausted()) {
            if (const auto offset = mode_parser.read_int<int64_t>()) {
                clock.offset_ = *offset;
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
            clock.rate_ = Fraction<int32_t> {*numerator, *denominator};
        } else {
            return tl::unexpected("media_clock: unexpected token");
        }
    } else {
        return tl::unexpected("media_clock: expecting rate");
    }

    return clock;
}

rav::sdp::MediaClockSource::ClockMode rav::sdp::MediaClockSource::mode() const {
    return mode_;
}

std::optional<int64_t> rav::sdp::MediaClockSource::offset() const {
    return offset_;
}

const std::optional<rav::Fraction<int>>& rav::sdp::MediaClockSource::rate() const {
    return rate_;
}

tl::expected<void, std::string> rav::sdp::MediaClockSource::validate() const {
    if (mode_ == ClockMode::undefined) {
        return tl::unexpected("media_clock: mode is undefined");
    }
    return {};
}

tl::expected<std::string, std::string> rav::sdp::MediaClockSource::to_string() const {
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

std::string rav::sdp::MediaClockSource::to_string(const ClockMode mode) {
    switch (mode) {
        case ClockMode::undefined:
            return "undefined";
        case ClockMode::direct:
        default:
            return "direct";
    }
}
