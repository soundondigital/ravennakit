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

tl::expected<void, std::string> rav::sdp::ravenna_clock_domain::validate() const {
    if (source == sync_source::undefined) {
        return tl::unexpected("clock_domain: sync source is undefined");
    }
    if (domain < 0) {
        return tl::unexpected("clock_domain: domain is negative");
    }
    return {};
}

tl::expected<std::string, std::string> rav::sdp::ravenna_clock_domain::to_string() const {
    auto validated = validate();
    if (!validated) {
        return tl::unexpected(validated.error());
    }
    return fmt::format("a={}:{} {}", k_attribute_name, to_string(source), domain);
}

std::string rav::sdp::ravenna_clock_domain::to_string(const sync_source source) {
    switch (source) {
        case sync_source::ptp_v2:
            return "PTPv2";
        case sync_source::undefined:
        default:
            return "undefined";
    }
}

rav::sdp::ravenna_clock_domain::parse_result<rav::sdp::ravenna_clock_domain>
rav::sdp::ravenna_clock_domain::parse_new(const std::string_view line) {
    string_parser parser(line);

    ravenna_clock_domain clock_domain;

    if (const auto sync_source = parser.split(' ')) {
        if (sync_source == "PTPv2") {
            if (const auto domain = parser.read_int<int32_t>()) {
                clock_domain = ravenna_clock_domain {sync_source::ptp_v2, *domain};
            } else {
                return parse_result<ravenna_clock_domain>::err("clock_domain: invalid domain");
            }
        } else {
            return parse_result<ravenna_clock_domain>::err("clock_domain: unsupported sync source");
        }
    } else {
        return parse_result<ravenna_clock_domain>::err("clock_domain: failed to parse sync source");
    }

    return parse_result<ravenna_clock_domain>::ok(clock_domain);
}
