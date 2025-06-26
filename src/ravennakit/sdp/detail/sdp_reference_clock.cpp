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
#include "ravennakit/sdp/detail/sdp_constants.hpp"

namespace {}  // namespace

const char* rav::sdp::to_string(const ReferenceClock::ClockSource source) {
    switch (source) {
        case ReferenceClock::ClockSource::atomic_clock:
            return "atomic-clock";
        case ReferenceClock::ClockSource::gps:
            return "gps";
        case ReferenceClock::ClockSource::terrestrial_radio:
            return "terrestrial-radio";
        case ReferenceClock::ClockSource::ptp:
            return "ptp";
        case ReferenceClock::ClockSource::ntp:
            return "ntp";
        case ReferenceClock::ClockSource::ntp_server:
            return "ntp-server";
        case ReferenceClock::ClockSource::ntp_pool:
            return "ntp-pool";
        case ReferenceClock::ClockSource::undefined:
        default:
            return "undefined";
    }
}

const char* rav::sdp::to_string(const ReferenceClock::PtpVersion version) {
    switch (version) {
        case ReferenceClock::PtpVersion::IEEE_1588_2002:
            return "IEEE1588-2002";
        case ReferenceClock::PtpVersion::IEEE_1588_2008:
            return "IEEE1588-2008";
        case ReferenceClock::PtpVersion::IEEE_802_1AS_2011:
            return "IEEE802.1AS-2011";
        case ReferenceClock::PtpVersion::traceable:
            return "traceable";
        case ReferenceClock::PtpVersion::undefined:
        default:
            return "undefined";
    }
}

std::string rav::sdp::to_string(const ReferenceClock& reference_clock) {
    if (reference_clock.source_ == ReferenceClock::ClockSource::ptp) {
        return fmt::format(
            "a={}:{}={}:{}:{}", k_sdp_ts_refclk, to_string(reference_clock.source_),
            to_string(reference_clock.ptp_version_.value()), reference_clock.gmid_.value(),
            reference_clock.domain_.value()
        );
    }
    // Note: this is not properly implemented:
    return fmt::format("a={}:{}", k_sdp_ts_refclk, to_string(reference_clock.source_));
}

tl::expected<rav::sdp::ReferenceClock, std::string> rav::sdp::parse_reference_clock(const std::string_view line) {
    StringParser parser(line);

    const auto source = parser.split("=");
    if (!source) {
        return tl::unexpected("reference_clock: invalid source");
    }

    if (source == "ptp") {
        ReferenceClock ref_clock;

        ref_clock.source_ = ReferenceClock::ClockSource::ptp;

        if (const auto ptp_version = parser.split(":")) {
            if (ptp_version == "IEEE1588-2002") {
                ref_clock.ptp_version_ = ReferenceClock::PtpVersion::IEEE_1588_2002;
            } else if (ptp_version == "IEEE1588-2008") {
                ref_clock.ptp_version_ = ReferenceClock::PtpVersion::IEEE_1588_2008;
            } else if (ptp_version == "IEEE802.1AS-2011") {
                ref_clock.ptp_version_ = ReferenceClock::PtpVersion::IEEE_802_1AS_2011;
            } else if (ptp_version == "traceable") {
                ref_clock.ptp_version_ = ReferenceClock::PtpVersion::traceable;
            } else {
                return tl::unexpected("reference_clock: unknown ptp version");
            }
        }

        if (const auto gmid = parser.split(":")) {
            ref_clock.gmid_ = *gmid;
        }

        if (parser.exhausted()) {
            return ref_clock;
        }

        if (const auto domain = parser.read_int<int32_t>()) {
            ref_clock.domain_ = *domain;
        } else {
            return tl::unexpected("reference_clock: invalid domain");
        }

        return ref_clock;
    }

    RAV_WARNING("reference_clock: ignoring clock source: {}", *source);

    return tl::unexpected("reference_clock: unsupported source");
}

tl::expected<void, std::string> rav::sdp::validate(const ReferenceClock& reference_clock) {
    if (reference_clock.source_ == ReferenceClock::ClockSource::ptp) {
        if (!reference_clock.ptp_version_) {
            return tl::unexpected("reference_clock: ptp version is undefined");
        }
        if (!reference_clock.gmid_) {
            return tl::unexpected("reference_clock: gmid is undefined");
        }
        if (!reference_clock.domain_) {
            return tl::unexpected("reference_clock: domain is undefined");
        }
    }
    if (reference_clock.source_ == ReferenceClock::ClockSource::undefined) {
        return tl::unexpected("reference_clock: source is undefined");
    }
    return {};
}
