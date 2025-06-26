/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/sdp_media_description.hpp"

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"
#include "ravennakit/sdp/detail/sdp_constants.hpp"
#include "ravennakit/sdp/detail/sdp_source_filter.hpp"

tl::expected<void, std::string> rav::sdp::MediaDescription::parse_attribute(const std::string_view line) {
    StringParser parser(line);

    if (!parser.skip("a=")) {
        return tl::unexpected("attribute: expecting 'a='");
    }

    auto key = parser.split(':');

    if (!key) {
        return tl::unexpected("attribute: expecting key");
    }

    if (key == k_sdp_rtp_map) {
        if (const auto value = parser.read_until_end()) {
            auto format = parse_format(*value);
            if (!format) {
                return tl::unexpected(format.error());
            }

            bool found = false;
            for (auto& fmt : formats) {
                if (fmt.payload_type == format->payload_type) {
                    fmt = std::move(*format);
                    found = true;
                    break;
                }
            }
            if (!found) {
                return tl::unexpected("media: rtpmap attribute for unknown payload type");
            }
        } else {
            return tl::unexpected("media: failed to parse rtpmap value");
        }
    } else if (key == k_sdp_ptime) {
        if (const auto value = parser.read_until_end()) {
            if (const auto new_ptime = string_to_float(value->data())) {
                if (*new_ptime < 0) {
                    return tl::unexpected("media: ptime must be a positive number");
                }
                ptime = *new_ptime;
            } else {
                return tl::unexpected("media: failed to parse ptime as double");
            }
        } else {
            return tl::unexpected("media: failed to parse ptime value");
        }
    } else if (key == k_sdp_max_ptime) {
        if (const auto value = parser.read_until_end()) {
            if (const auto maxptime = string_to_float(value->data())) {
                if (*maxptime < 0) {
                    return tl::unexpected("media: maxptime must be a positive number");
                }
                max_ptime = *maxptime;
            } else {
                return tl::unexpected("media: failed to parse ptime as double");
            }
        } else {
            return tl::unexpected("media: failed to parse maxptime value");
        }
    } else if (key == k_sdp_sendrecv) {
        media_direction = MediaDirection::sendrecv;
    } else if (key == k_sdp_sendonly) {
        media_direction = MediaDirection::sendonly;
    } else if (key == k_sdp_recvonly) {
        media_direction = MediaDirection::recvonly;
    } else if (key == k_sdp_inactive) {
        media_direction = MediaDirection::inactive;
    } else if (key == k_sdp_ts_refclk) {
        if (const auto value = parser.read_until_end()) {
            auto ref_clock = parse_reference_clock(*value);
            if (!ref_clock) {
                return tl::unexpected(ref_clock.error());
            }
            reference_clock = std::move(*ref_clock);
        } else {
            return tl::unexpected("media: failed to parse ts-refclk value");
        }
    } else if (key == MediaClockSource::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock = parse_media_clock_source(*value);
            if (!clock) {
                return tl::unexpected(clock.error());
            }
            media_clock = *clock;
        } else {
            return tl::unexpected("media: failed to parse media clock value");
        }
    } else if (key == RavennaClockDomain::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock_domain = parse_ravenna_clock_domain(*value);
            if (!clock_domain) {
                return tl::unexpected(clock_domain.error());
            }
            ravenna_clock_domain = *clock_domain;
        } else {
            return tl::unexpected("media: failed to parse clock domain value");
        }
    } else if (key == k_sdp_sync_time) {
        if (const auto rtp_ts = parser.read_int<uint32_t>()) {
            ravenna_sync_time = *rtp_ts;
        } else {
            return tl::unexpected("media: failed to parse sync-time value");
        }
    } else if (key == k_sdp_clock_deviation) {
        const auto num = parser.read_int<uint32_t>();
        if (!num) {
            return tl::unexpected("media: failed to parse clock-deviation value");
        }
        if (!parser.skip('/')) {
            return tl::unexpected("media: expecting '/' after clock-deviation numerator value");
        }
        const auto denom = parser.read_int<uint32_t>();
        if (!denom) {
            return tl::unexpected("media: failed to parse clock-deviation denominator value");
        }
        ravenna_clock_deviation = Fraction<uint32_t> {*num, *denom};
    } else if (key == SourceFilter::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto filter = parse_source_filter(*value);
            if (!filter) {
                return tl::unexpected(filter.error());
            }
            source_filters.push_back(std::move(*filter));
        } else {
            return tl::unexpected("media: failed to parse source-filter value");
        }
    } else if (key == "framecount") {
        if (auto value = parser.read_int<uint16_t>()) {
            ravenna_framecount = *value;
        } else {
            return tl::unexpected("media: failed to parse framecount value");
        }
    } else if (key == k_sdp_mid) {
        auto new_mid = parser.read_until_end();
        if (!new_mid) {
            return tl::unexpected("media: failed to parse mid value");
        }
        if (new_mid->empty()) {
            return tl::unexpected("media: mid value cannot be empty");
        }
        mid = *new_mid;
    } else {
        // Store the attribute in the map of unknown attributes
        if (auto value = parser.read_until_end()) {
            attributes.emplace(*key, *value);
        } else {
            return tl::unexpected("media: failed to parse attribute value");
        }
    }

    return {};
}

void rav::sdp::MediaDescription::add_or_update_format(const Format& format_to_add) {
    for (auto& fmt : formats) {
        if (fmt.payload_type == format_to_add.payload_type) {
            fmt = format_to_add;
            return;
        }
    }
    formats.push_back(format_to_add);
}

void rav::sdp::MediaDescription::add_or_update_source_filter(const SourceFilter& filter) {
    for (auto& f : source_filters) {
        if (f.net_type == filter.net_type && f.addr_type == filter.addr_type && f.dest_address == filter.dest_address) {
            f = filter;
            return;
        }
    }

    source_filters.push_back(filter);
}

tl::expected<void, std::string> rav::sdp::validate(const MediaDescription& media) {
    if (media.media_type.empty()) {
        return tl::make_unexpected("media: media type is empty");
    }
    if (media.port == 0) {
        return tl::make_unexpected("media: port is 0");
    }
    if (media.number_of_ports == 0) {
        return tl::make_unexpected("media: number of ports is 0");
    }
    if (media.protocol.empty()) {
        return tl::make_unexpected("media: protocol is empty");
    }
    if (media.formats.empty()) {
        return tl::make_unexpected("media: no formats specified");
    }
    return {};
}

std::string rav::sdp::to_string(const MediaDescription& media_description, const char* newline) {
    // Media line
    std::string result;
    fmt::format_to(std::back_inserter(result), "m={} {}", media_description.media_type, media_description.port);
    if (media_description.number_of_ports > 1) {
        fmt::format_to(std::back_inserter(result), "/{}", media_description.number_of_ports);
    }
    fmt::format_to(std::back_inserter(result), " {}", media_description.protocol);

    for (auto& fmt : media_description.formats) {
        fmt::format_to(std::back_inserter(result), " {}", fmt.payload_type);
    }

    fmt::format_to(std::back_inserter(result), "{}", newline);

    // Connection info
    for (const auto& conn : media_description.connection_infos) {
        fmt::format_to(std::back_inserter(result), "{}{}", sdp::to_string(conn), newline);
    }

    // Session information
    if (media_description.session_information) {
        fmt::format_to(std::back_inserter(result), "s={}{}", *media_description.session_information, newline);
    }

    // rtpmaps
    for (const auto& fmt : media_description.formats) {
        fmt::format_to(std::back_inserter(result), "a=rtpmap:{}{}", sdp::to_string(fmt), newline);
    }

    // ptime
    if (media_description.ptime) {
        fmt::format_to(std::back_inserter(result), "a=ptime:{:.3g}{}", *media_description.ptime, newline);
    }

    // max_ptime
    if (media_description.max_ptime) {
        fmt::format_to(std::back_inserter(result), "a=maxptime:{:.3g}{}", *media_description.max_ptime, newline);
    }

    // Group duplication
    if (media_description.mid) {
        RAV_ASSERT(!media_description.mid->empty(), "media: mid value cannot be empty");
        fmt::format_to(std::back_inserter(result), "a=mid:{}{}", *media_description.mid, newline);
    }

    // Media direction
    if (media_description.media_direction) {
        fmt::format_to(
            std::back_inserter(result), "a={}{}", sdp::to_string(*media_description.media_direction), newline
        );
    }

    // Reference clock
    if (media_description.reference_clock) {
        fmt::format_to(std::back_inserter(result), "{}{}", sdp::to_string(*media_description.reference_clock), newline);
    }

    // Media clock
    if (media_description.media_clock) {
        fmt::format_to(std::back_inserter(result), "{}{}", sdp::to_string(*media_description.media_clock), newline);
    }

    // Clock domain (RAVENNA Specific)
    if (media_description.ravenna_clock_domain) {
        fmt::format_to(std::back_inserter(result), "{}{}", to_string(*media_description.ravenna_clock_domain), newline);
    }

    // Sync time (RAVENNA Specific)
    if (media_description.ravenna_sync_time) {
        fmt::format_to(std::back_inserter(result), "a=sync-time:{}{}", *media_description.ravenna_sync_time, newline);
    }

    // Clock deviation (RAVENNA Specific)
    if (media_description.ravenna_clock_deviation) {
        fmt::format_to(
            std::back_inserter(result), "a=clock-deviation:{}/{}{}",
            media_description.ravenna_clock_deviation->numerator,
            media_description.ravenna_clock_deviation->denominator, newline
        );
    }

    // Source filters
    for (auto& filter : media_description.source_filters) {
        fmt::format_to(std::back_inserter(result), "{}{}", sdp::to_string(filter), newline);
    }

    // Framecount (legacy RAVENNA)
    if (media_description.ravenna_framecount) {
        fmt::format_to(std::back_inserter(result), "a=framecount:{}{}", *media_description.ravenna_framecount, newline);
    }

    return result;
}

tl::expected<rav::sdp::MediaDescription, std::string> rav::sdp::parse_media_description(const std::string_view line) {
    StringParser parser(line);

    if (!parser.skip("m=")) {
        return tl::unexpected("media: expecting 'm='");
    }

    MediaDescription media;

    // Media type
    if (const auto media_type = parser.split(' ')) {
        media.media_type = *media_type;
    } else {
        return tl::unexpected("media: failed to parse media type");
    }

    // Port
    if (const auto port = parser.read_int<uint16_t>()) {
        media.port = *port;
        if (parser.skip('/')) {
            if (const auto num_ports = parser.read_int<uint16_t>()) {
                media.number_of_ports = *num_ports;
            } else {
                return tl::unexpected("media: failed to parse number of ports as integer");
            }
        } else {
            media.number_of_ports = 1;
        }
        parser.skip(' ');
    } else {
        return tl::unexpected("media: failed to parse port as integer");
    }

    // Protocol
    if (const auto protocol = parser.split(' ')) {
        media.protocol = *protocol;
    } else {
        return tl::unexpected("media: failed to parse protocol");
    }

    // Formats
    while (const auto format_str = parser.split(' ')) {
        if (const auto value = rav::string_to_int<uint8_t>(*format_str)) {
            media.formats.push_back({*value, {}, {}, {}});
        } else {
            return tl::unexpected("media: format integer parsing failed");
        }
    }

    return media;
}
