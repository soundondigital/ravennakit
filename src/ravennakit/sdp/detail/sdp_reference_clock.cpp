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

rav::sdp::ReferenceClock::ReferenceClock(
    const ClockSource source, PtpVersion version, std::string gmid, int32_t domain
) :
    source_(source), ptp_version_(version), gmid_(gmid), domain_(domain) {}

rav::sdp::ReferenceClock::ClockSource rav::sdp::ReferenceClock::source() const {
    return source_;
}

std::optional<rav::sdp::ReferenceClock::PtpVersion> rav::sdp::ReferenceClock::ptp_version() const {
    return ptp_version_;
}

const std::optional<std::string>& rav::sdp::ReferenceClock::gmid() const {
    return gmid_;
}

const std::optional<int32_t>& rav::sdp::ReferenceClock::domain() const {
    return domain_;
}

tl::expected<void, std::string> rav::sdp::ReferenceClock::validate() const {
    if (source_ == ClockSource::ptp) {
        if (!ptp_version_) {
            return tl::unexpected("reference_clock: ptp version is undefined");
        }
        if (!gmid_) {
            return tl::unexpected("reference_clock: gmid is undefined");
        }
        if (!domain_) {
            return tl::unexpected("reference_clock: domain is undefined");
        }
    }
    if (source_ == ClockSource::undefined) {
        return tl::unexpected("reference_clock: source is undefined");
    }
    return {};
}

tl::expected<std::string, std::string> rav::sdp::ReferenceClock::to_string() const {
    auto validated = validate();
    if (!validated) {
        return tl::unexpected(validated.error());
    }
    if (source_ == ClockSource::ptp) {
        return fmt::format(
            "a={}:{}={}:{}:{}", k_sdp_ts_refclk, to_string(source_), to_string(ptp_version_.value()), gmid_.value(),
            domain_.value()
        );
    }
    // Note: this is not properly implemented:
    return fmt::format("a={}:{}", k_sdp_ts_refclk, to_string(source_));
}

rav::sdp::ReferenceClock::ParseResult<rav::sdp::ReferenceClock>
rav::sdp::ReferenceClock::parse_new(const std::string_view line) {
    string_parser parser(line);

    const auto source = parser.split("=");
    if (!source) {
        return ParseResult<ReferenceClock>::err("reference_clock: invalid source");
    }

    if (source == "ptp") {
        ReferenceClock ref_clock;

        ref_clock.source_ = ClockSource::ptp;

        if (const auto ptp_version = parser.split(":")) {
            if (ptp_version == "IEEE1588-2002") {
                ref_clock.ptp_version_ = PtpVersion::IEEE_1588_2002;
            } else if (ptp_version == "IEEE1588-2008") {
                ref_clock.ptp_version_ = PtpVersion::IEEE_1588_2008;
            } else if (ptp_version == "IEEE802.1AS-2011") {
                ref_clock.ptp_version_ = PtpVersion::IEEE_802_1AS_2011;
            } else if (ptp_version == "traceable") {
                ref_clock.ptp_version_ = PtpVersion::traceable;
            } else {
                return ParseResult<ReferenceClock>::err("reference_clock: unknown ptp version");
            }
        }

        if (const auto gmid = parser.split(":")) {
            ref_clock.gmid_ = *gmid;
        }

        if (parser.exhausted()) {
            return ParseResult<ReferenceClock>::ok(std::move(ref_clock));
        }

        if (const auto domain = parser.read_int<int32_t>()) {
            ref_clock.domain_ = *domain;
        } else {
            return ParseResult<ReferenceClock>::err("reference_clock: invalid domain");
        }

        return ParseResult<ReferenceClock>::ok(std::move(ref_clock));
    }

    RAV_WARNING("reference_clock: ignoring clock source: {}", *source);

    return ParseResult<ReferenceClock>::err("reference_clock: unsupported source");
}

std::string rav::sdp::ReferenceClock::to_string(const ClockSource source) {
    switch (source) {
        case ClockSource::atomic_clock:
            return "atomic-clock";
        case ClockSource::gps:
            return "gps";
        case ClockSource::terrestrial_radio:
            return "terrestrial-radio";
        case ClockSource::ptp:
            return "ptp";
        case ClockSource::ntp:
            return "ntp";
        case ClockSource::ntp_server:
            return "ntp-server";
        case ClockSource::ntp_pool:
            return "ntp-pool";
        case ClockSource::undefined:
        default:
            return "undefined";
    }
}

std::string rav::sdp::ReferenceClock::to_string(const PtpVersion version) {
    switch (version) {
        case PtpVersion::IEEE_1588_2002:
            return "IEEE1588-2002";
        case PtpVersion::IEEE_1588_2008:
            return "IEEE1588-2008";
        case PtpVersion::IEEE_802_1AS_2011:
            return "IEEE802.1AS-2011";
        case PtpVersion::traceable:
            return "traceable";
        case PtpVersion::undefined:
        default:
            return "undefined";
    }
}
