/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_ravenna_clock_domain.hpp"

#include "ravennakit/core/string_parser.hpp"

tl::expected<rav::sdp::RavennaClockDomain, std::string>
rav::sdp::parse_ravenna_clock_domain(const std::string_view line) {
    StringParser parser(line);

    RavennaClockDomain clock_domain;

    if (const auto sync_source = parser.split(' ')) {
        if (sync_source == "PTPv2") {
            if (const auto domain = parser.read_int<int32_t>()) {
                clock_domain = RavennaClockDomain {RavennaClockDomain::SyncSource::ptp_v2, *domain};
            } else {
                return tl::unexpected("clock_domain: invalid domain");
            }
        } else {
            return tl::unexpected("clock_domain: unsupported sync source");
        }
    } else {
        return tl::unexpected("clock_domain: failed to parse sync source");
    }

    return clock_domain;
}

const char* rav::sdp::to_string(const RavennaClockDomain::SyncSource source) {
    switch (source) {
        case RavennaClockDomain::SyncSource::ptp_v2:
            return "PTPv2";
        case RavennaClockDomain::SyncSource::undefined:
        default:
            return "undefined";
    }
}

tl::expected<std::string, std::string> rav::sdp::to_string(const RavennaClockDomain& ravenna_clock_domain) {
    return fmt::format(
        "a={}:{} {}", RavennaClockDomain::k_attribute_name, to_string(ravenna_clock_domain.source),
        ravenna_clock_domain.domain
    );
}

tl::expected<void, std::string> rav::sdp::validate(const RavennaClockDomain& ravenna_clock_domain) {
    if (ravenna_clock_domain.source == RavennaClockDomain::SyncSource::undefined) {
        return tl::unexpected("clock_domain: sync source is undefined");
    }
    if (ravenna_clock_domain.domain < 0) {
        return tl::unexpected("clock_domain: domain is negative");
    }
    return {};
}
