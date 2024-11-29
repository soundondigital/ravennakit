/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_reference_clock.hpp"

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/string.hpp"
#include "ravennakit/core/string_parser.hpp"

namespace {}  // namespace

rav::sdp::reference_clock::clock_source rav::sdp::reference_clock::source() const {
    return source_;
}

std::optional<rav::sdp::reference_clock::ptp_ver> rav::sdp::reference_clock::ptp_version() const {
    return ptp_version_;
}

const std::optional<std::string>& rav::sdp::reference_clock::gmid() const {
    return gmid_;
}

const std::optional<int32_t>& rav::sdp::reference_clock::domain() const {
    return domain_;
}

rav::sdp::reference_clock::parse_result<rav::sdp::reference_clock>
rav::sdp::reference_clock::parse_new(const std::string_view line) {
    string_parser parser(line);

    const auto source = parser.split("=");
    if (!source) {
        return parse_result<reference_clock>::err("reference_clock: invalid source");
    }

    if (source == "ptp") {
        reference_clock ref_clock;

        ref_clock.source_ = clock_source::ptp;

        if (const auto ptp_version = parser.split(":")) {
            if (ptp_version == "IEEE1588-2002") {
                ref_clock.ptp_version_ = ptp_ver::IEEE_1588_2002;
            } else if (ptp_version == "IEEE1588-2008") {
                ref_clock.ptp_version_ = ptp_ver::IEEE_1588_2008;
            } else if (ptp_version == "IEEE802.1AS-2011") {
                ref_clock.ptp_version_ = ptp_ver::IEEE_802_1AS_2011;
            } else if (ptp_version == "traceable") {
                ref_clock.ptp_version_ = ptp_ver::traceable;
            } else {
                return parse_result<reference_clock>::err("reference_clock: unknown ptp version");
            }
        }

        if (const auto gmid = parser.split(":")) {
            ref_clock.gmid_ = *gmid;
        }

        if (parser.exhausted()) {
            return parse_result<reference_clock>::ok(std::move(ref_clock));
        }

        if (const auto domain = parser.read_int<int32_t>()) {
            ref_clock.domain_ = *domain;
        } else {
            return parse_result<reference_clock>::err("reference_clock: invalid domain");
        }

        return parse_result<reference_clock>::ok(std::move(ref_clock));
    }

    RAV_WARNING("reference_clock: ignoring clock source: {}", *source);

    return parse_result<reference_clock>::err("reference_clock: unsupported source");
}
