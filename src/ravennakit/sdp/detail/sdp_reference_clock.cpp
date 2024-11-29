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

rav::sdp::reference_clock::reference_clock(
    const clock_source source, ptp_ver version, std::string gmid, int32_t domain
) :
    source_(source), ptp_version_(version), gmid_(gmid), domain_(domain) {}

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

tl::expected<void, std::string> rav::sdp::reference_clock::validate() const {
    if (source_ == clock_source::ptp) {
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
    if (source_ == clock_source::undefined) {
        return tl::unexpected("reference_clock: source is undefined");
    }
    return {};
}

tl::expected<std::string, std::string> rav::sdp::reference_clock::to_string() const {
    auto validated = validate();
    if (!validated) {
        return tl::unexpected(validated.error());
    }
    if (source_ == clock_source::ptp) {
        return fmt::format(
            "a={}:{}={}:{}:{}", k_sdp_ts_refclk, to_string(source_), to_string(ptp_version_.value()), gmid_.value(),
            domain_.value()
        );
    }
    // Note: this is not properly implemented:
    return fmt::format("a={}:{}", k_sdp_ts_refclk, to_string(source_));
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

std::string rav::sdp::reference_clock::to_string(const clock_source source) {
    switch (source) {
        case clock_source::atomic_clock:
            return "atomic-clock";
        case clock_source::gps:
            return "gps";
        case clock_source::terrestrial_radio:
            return "terrestrial-radio";
        case clock_source::ptp:
            return "ptp";
        case clock_source::ntp:
            return "ntp";
        case clock_source::ntp_server:
            return "ntp-server";
        case clock_source::ntp_pool:
            return "ntp-pool";
        case clock_source::undefined:
        default:
            return "undefined";
    }
}

std::string rav::sdp::reference_clock::to_string(const ptp_ver version) {
    switch (version) {
        case ptp_ver::IEEE_1588_2002:
            return "IEEE1588-2002";
        case ptp_ver::IEEE_1588_2008:
            return "IEEE1588-2008";
        case ptp_ver::IEEE_802_1AS_2011:
            return "IEEE802.1AS-2011";
        case ptp_ver::traceable:
            return "traceable";
        case ptp_ver::undefined:
        default:
            return "undefined";
    }
}
