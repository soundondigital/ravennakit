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

tl::expected<void, std::string> rav::sdp::RavennaClockDomain::validate() const {
    if (source == SyncSource::undefined) {
        return tl::unexpected("clock_domain: sync source is undefined");
    }
    if (domain < 0) {
        return tl::unexpected("clock_domain: domain is negative");
    }
    return {};
}

tl::expected<std::string, std::string> rav::sdp::RavennaClockDomain::to_string() const {
    auto validated = validate();
    if (!validated) {
        return tl::unexpected(validated.error());
    }
    return fmt::format("a={}:{} {}", k_attribute_name, to_string(source), domain);
}

std::string rav::sdp::RavennaClockDomain::to_string(const SyncSource source) {
    switch (source) {
        case SyncSource::ptp_v2:
            return "PTPv2";
        case SyncSource::undefined:
        default:
            return "undefined";
    }
}

tl::expected<rav::sdp::RavennaClockDomain, std::string>
rav::sdp::RavennaClockDomain::parse_new(const std::string_view line) {
    StringParser parser(line);

    RavennaClockDomain clock_domain;

    if (const auto sync_source = parser.split(' ')) {
        if (sync_source == "PTPv2") {
            if (const auto domain = parser.read_int<int32_t>()) {
                clock_domain = RavennaClockDomain {SyncSource::ptp_v2, *domain};
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
