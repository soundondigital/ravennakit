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

rav::sdp::MediaDescription::ParseResult<rav::sdp::MediaDescription>
rav::sdp::MediaDescription::parse_new(const std::string_view line) {
    StringParser parser(line);

    if (!parser.skip("m=")) {
        return ParseResult<MediaDescription>::err("media: expecting 'm='");
    }

    MediaDescription media;

    // Media type
    if (const auto media_type = parser.split(' ')) {
        media.media_type_ = *media_type;
    } else {
        return ParseResult<MediaDescription>::err("media: failed to parse media type");
    }

    // Port
    if (const auto port = parser.read_int<uint16_t>()) {
        media.port_ = *port;
        if (parser.skip('/')) {
            if (const auto num_ports = parser.read_int<uint16_t>()) {
                media.number_of_ports_ = *num_ports;
            } else {
                return ParseResult<MediaDescription>::err("media: failed to parse number of ports as integer");
            }
        } else {
            media.number_of_ports_ = 1;
        }
        parser.skip(' ');
    } else {
        return ParseResult<MediaDescription>::err("media: failed to parse port as integer");
    }

    // Protocol
    if (const auto protocol = parser.split(' ')) {
        media.protocol_ = *protocol;
    } else {
        return ParseResult<MediaDescription>::err("media: failed to parse protocol");
    }

    // Formats
    while (const auto format_str = parser.split(' ')) {
        if (const auto value = rav::string_to_int<uint8_t>(*format_str)) {
            media.formats_.push_back({*value, {}, {}, {}});
        } else {
            return ParseResult<MediaDescription>::err("media: format integer parsing failed");
        }
    }

    return ParseResult<MediaDescription>::ok(std::move(media));
}

rav::sdp::MediaDescription::ParseResult<void> rav::sdp::MediaDescription::parse_attribute(const std::string_view line) {
    StringParser parser(line);

    if (!parser.skip("a=")) {
        return ParseResult<void>::err("attribute: expecting 'a='");
    }

    auto key = parser.split(':');

    if (!key) {
        return ParseResult<void>::err("attribute: expecting key");
    }

    if (key == k_sdp_rtp_map) {
        if (const auto value = parser.read_until_end()) {
            auto format_result = Format::parse_new(*value);
            if (format_result.is_err()) {
                return ParseResult<void>::err(format_result.get_err());
            }

            auto format = format_result.move_ok();

            bool found = false;
            for (auto& fmt : formats_) {
                if (fmt.payload_type == format.payload_type) {
                    fmt = format;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return ParseResult<void>::err("media: rtpmap attribute for unknown payload type");
            }
        } else {
            return ParseResult<void>::err("media: failed to parse rtpmap value");
        }
    } else if (key == k_sdp_ptime) {
        if (const auto value = parser.read_until_end()) {
            if (const auto ptime = string_to_float(value->data())) {
                if (*ptime < 0) {
                    return ParseResult<void>::err("media: ptime must be a positive number");
                }
                ptime_ = *ptime;
            } else {
                return ParseResult<void>::err("media: failed to parse ptime as double");
            }
        } else {
            return ParseResult<void>::err("media: failed to parse ptime value");
        }
    } else if (key == k_sdp_max_ptime) {
        if (const auto value = parser.read_until_end()) {
            if (const auto maxptime = string_to_float(value->data())) {
                if (*maxptime < 0) {
                    return ParseResult<void>::err("media: maxptime must be a positive number");
                }
                max_ptime_ = *maxptime;
            } else {
                return ParseResult<void>::err("media: failed to parse ptime as double");
            }
        } else {
            return ParseResult<void>::err("media: failed to parse maxptime value");
        }
    } else if (key == k_sdp_sendrecv) {
        media_direction_ = MediaDirection::sendrecv;
    } else if (key == k_sdp_sendonly) {
        media_direction_ = MediaDirection::sendonly;
    } else if (key == k_sdp_recvonly) {
        media_direction_ = MediaDirection::recvonly;
    } else if (key == k_sdp_inactive) {
        media_direction_ = MediaDirection::inactive;
    } else if (key == k_sdp_ts_refclk) {
        if (const auto value = parser.read_until_end()) {
            auto ref_clock = sdp::ReferenceClock::parse_new(*value);
            if (ref_clock.is_err()) {
                return ParseResult<void>::err(ref_clock.get_err());
            }
            reference_clock_ = ref_clock.move_ok();
        } else {
            return ParseResult<void>::err("media: failed to parse ts-refclk value");
        }
    } else if (key == sdp::MediaClockSource::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock = sdp::MediaClockSource::parse_new(*value);
            if (clock.is_err()) {
                return ParseResult<void>::err(clock.get_err());
            }
            media_clock_ = clock.move_ok();
        } else {
            return ParseResult<void>::err("media: failed to parse media clock value");
        }
    } else if (key == RavennaClockDomain::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock_domain = RavennaClockDomain::parse_new(*value);
            if (clock_domain.is_err()) {
                return ParseResult<void>::err(clock_domain.get_err());
            }
            clock_domain_ = clock_domain.move_ok();
        } else {
            return ParseResult<void>::err("media: failed to parse clock domain value");
        }
    } else if (key == k_sdp_sync_time) {
        if (const auto rtp_ts = parser.read_int<uint32_t>()) {
            sync_time_ = *rtp_ts;
        } else {
            return ParseResult<void>::err("media: failed to parse sync-time value");
        }
    } else if (key == k_sdp_clock_deviation) {
        const auto num = parser.read_int<uint32_t>();
        if (!num) {
            return ParseResult<void>::err("media: failed to parse clock-deviation value");
        }
        if (!parser.skip('/')) {
            return ParseResult<void>::err("media: expecting '/' after clock-deviation numerator value");
        }
        const auto denom = parser.read_int<uint32_t>();
        if (!denom) {
            return ParseResult<void>::err("media: failed to parse clock-deviation denominator value");
        }
        clock_deviation_ = Fraction<uint32_t> {*num, *denom};
    } else if (key == SourceFilter::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto filter = SourceFilter::parse_new(*value);
            if (filter.is_err()) {
                return ParseResult<void>::err(filter.get_err());
            }
            source_filters_.push_back(filter.move_ok());
        } else {
            return ParseResult<void>::err("media: failed to parse source-filter value");
        }
    } else if (key == "framecount") {
        if (auto value = parser.read_int<uint16_t>()) {
            framecount_ = *value;
        } else {
            return ParseResult<void>::err("media: failed to parse framecount value");
        }
    } else if (key == k_sdp_mid) {
        auto mid = parser.read_until_end();
        if (!mid) {
            return ParseResult<void>::err("media: failed to parse mid value");
        }
        if (mid->empty()) {
            return ParseResult<void>::err("media: mid value cannot be empty");
        }
        mid_ = *mid;
    } else {
        // Store the attribute in the map of unknown attributes
        if (auto value = parser.read_until_end()) {
            attributes_.emplace(*key, *value);
        } else {
            return ParseResult<void>::err("media: failed to parse attribute value");
        }
    }

    return ParseResult<void>::ok();
}

const std::string& rav::sdp::MediaDescription::media_type() const {
    return media_type_;
}

void rav::sdp::MediaDescription::set_media_type(std::string media_type) {
    media_type_ = std::move(media_type);
}

uint16_t rav::sdp::MediaDescription::port() const {
    return port_;
}

void rav::sdp::MediaDescription::set_port(uint16_t port) {
    port_ = port;
}

uint16_t rav::sdp::MediaDescription::number_of_ports() const {
    return number_of_ports_;
}

void rav::sdp::MediaDescription::set_number_of_ports(const uint16_t number_of_ports) {
    if (number_of_ports == 0) {
        RAV_ASSERT_FALSE("media: number of ports cannot be 0, setting to 1");
        number_of_ports_ = 1;
        return;
    }
    number_of_ports_ = number_of_ports;
}

const std::string& rav::sdp::MediaDescription::protocol() const {
    return protocol_;
}

void rav::sdp::MediaDescription::set_protocol(std::string protocol) {
    protocol_ = std::move(protocol);
}

const std::vector<rav::sdp::Format>& rav::sdp::MediaDescription::formats() const {
    return formats_;
}

void rav::sdp::MediaDescription::add_format(const Format& format_to_add) {
    for (auto& fmt : formats_) {
        if (fmt.payload_type == format_to_add.payload_type) {
            fmt = format_to_add;
            return;
        }
    }
    formats_.push_back(format_to_add);
}

const std::vector<rav::sdp::ConnectionInfoField>& rav::sdp::MediaDescription::connection_infos() const {
    return connection_infos_;
}

void rav::sdp::MediaDescription::add_connection_info(ConnectionInfoField connection_info) {
    connection_infos_.push_back(std::move(connection_info));
}

void rav::sdp::MediaDescription::set_session_information(std::string session_information) {
    session_information_ = std::move(session_information);
}

std::optional<float> rav::sdp::MediaDescription::ptime() const {
    return ptime_;
}

void rav::sdp::MediaDescription::set_ptime(const std::optional<float> ptime) {
    ptime_ = ptime;
}

std::optional<float> rav::sdp::MediaDescription::max_ptime() const {
    return max_ptime_;
}

void rav::sdp::MediaDescription::set_max_ptime(const std::optional<float> max_ptime) {
    max_ptime_ = max_ptime;
}

const std::optional<rav::sdp::MediaDirection>& rav::sdp::MediaDescription::direction() const {
    return media_direction_;
}

void rav::sdp::MediaDescription::set_direction(MediaDirection direction) {
    media_direction_ = direction;
}

const std::optional<rav::sdp::ReferenceClock>& rav::sdp::MediaDescription::ref_clock() const {
    return reference_clock_;
}

void rav::sdp::MediaDescription::set_ref_clock(ReferenceClock ref_clock) {
    reference_clock_ = std::move(ref_clock);
}

const std::optional<rav::sdp::MediaClockSource>& rav::sdp::MediaDescription::media_clock() const {
    return media_clock_;
}

void rav::sdp::MediaDescription::set_media_clock(MediaClockSource media_clock) {
    media_clock_ = std::move(media_clock);
}

const std::optional<std::string>& rav::sdp::MediaDescription::session_information() const {
    return session_information_;
}

std::optional<uint32_t> rav::sdp::MediaDescription::sync_time() const {
    return sync_time_;
}

void rav::sdp::MediaDescription::set_sync_time(const std::optional<uint32_t> sync_time) {
    sync_time_ = sync_time;
}

const std::optional<rav::Fraction<unsigned>>& rav::sdp::MediaDescription::clock_deviation() const {
    return clock_deviation_;
}

void rav::sdp::MediaDescription::set_clock_deviation(const std::optional<Fraction<uint32_t>> clock_deviation) {
    clock_deviation_ = clock_deviation;
}

const std::vector<rav::sdp::SourceFilter>& rav::sdp::MediaDescription::source_filters() const {
    return source_filters_;
}

void rav::sdp::MediaDescription::add_source_filter(const SourceFilter& filter) {
    for (auto& f : source_filters_) {
        if (f.network_type() == filter.network_type() && f.address_type() == filter.address_type()
            && f.dest_address() == filter.dest_address()) {
            f = filter;
            return;
        }
    }

    source_filters_.push_back(filter);
}

std::optional<uint16_t> rav::sdp::MediaDescription::framecount() const {
    return framecount_;
}

void rav::sdp::MediaDescription::set_framecount(const std::optional<uint32_t> framecount) {
    framecount_ = framecount;
}

std::optional<rav::sdp::RavennaClockDomain> rav::sdp::MediaDescription::clock_domain() const {
    return clock_domain_;
}

void rav::sdp::MediaDescription::set_clock_domain(RavennaClockDomain clock_domain) {
    clock_domain_ = clock_domain;
}

const std::optional<std::string>& rav::sdp::MediaDescription::get_mid() const {
    return mid_;
}

void rav::sdp::MediaDescription::set_mid(std::optional<std::string> mid) {
    mid_ = std::move(mid);
}

const std::map<std::string, std::string>& rav::sdp::MediaDescription::attributes() const {
    return attributes_;
}

tl::expected<void, std::string> rav::sdp::MediaDescription::validate() const {
    if (media_type_.empty()) {
        return tl::make_unexpected("media: media type is empty");
    }
    if (port_ == 0) {
        return tl::make_unexpected("media: port is 0");
    }
    if (number_of_ports_ == 0) {
        return tl::make_unexpected("media: number of ports is 0");
    }
    if (protocol_.empty()) {
        return tl::make_unexpected("media: protocol is empty");
    }
    if (formats_.empty()) {
        return tl::make_unexpected("media: no formats specified");
    }
    return {};
}

tl::expected<std::string, std::string> rav::sdp::MediaDescription::to_string(const char* newline) const {
    auto validated = validate();
    if (!validated) {
        return tl::make_unexpected(validated.error());
    }

    // Media line
    std::string result;
    fmt::format_to(std::back_inserter(result), "m={} {}", media_type_, port_);
    if (number_of_ports_ > 1) {
        fmt::format_to(std::back_inserter(result), "/{}", number_of_ports_);
    }
    fmt::format_to(std::back_inserter(result), " {}", protocol_);

    for (auto& fmt : formats_) {
        fmt::format_to(std::back_inserter(result), " {}", fmt.payload_type);
    }

    fmt::format_to(std::back_inserter(result), "{}", newline);

    // Connection info
    for (const auto& conn : connection_infos_) {
        const auto txt = conn.to_string();
        if (!txt) {
            return tl::make_unexpected(validated.error());
        }
        fmt::format_to(std::back_inserter(result), "{}{}", txt.value(), newline);
    }

    // Session information
    if (session_information_) {
        fmt::format_to(std::back_inserter(result), "s={}{}", *session_information_, newline);
    }

    // rtpmaps
    for (const auto& fmt : formats_) {
        fmt::format_to(std::back_inserter(result), "a=rtpmap:{}{}", fmt.to_string(), newline);
    }

    // ptime
    if (ptime_) {
        fmt::format_to(std::back_inserter(result), "a=ptime:{:.3g}{}", *ptime_, newline);
    }

    // max_ptime
    if (max_ptime_) {
        fmt::format_to(std::back_inserter(result), "a=maxptime:{:.3g}{}", *max_ptime_, newline);
    }

    // Group duplication
    if (mid_) {
        RAV_ASSERT(!mid_->empty(), "media: mid value cannot be empty");
        fmt::format_to(std::back_inserter(result), "a=mid:{}{}", *mid_, newline);
    }

    // Media direction
    if (media_direction_) {
        fmt::format_to(std::back_inserter(result), "a={}{}", sdp::to_string(*media_direction_), newline);
    }

    // Reference clock
    if (reference_clock_) {
        auto refclk = reference_clock_->to_string();
        if (!refclk) {
            return tl::make_unexpected(refclk.error());
        }
        fmt::format_to(std::back_inserter(result), "{}{}", refclk.value(), newline);
    }

    // Media clock
    if (media_clock_) {
        auto clock = media_clock_->to_string();
        if (!clock) {
            return tl::make_unexpected(clock.error());
        }
        fmt::format_to(std::back_inserter(result), "{}{}", clock.value(), newline);
    }

    // Clock domain (RAVENNA Specific)
    if (clock_domain_) {
        auto txt = clock_domain_->to_string();
        if (!txt) {
            return tl::make_unexpected(txt.error());
        }
        fmt::format_to(std::back_inserter(result), "{}{}", txt.value(), newline);
    }

    // Sync time (RAVENNA Specific)
    if (sync_time_) {
        fmt::format_to(std::back_inserter(result), "a=sync-time:{}{}", *sync_time_, newline);
    }

    // Clock deviation (RAVENNA Specific)
    if (clock_deviation_) {
        fmt::format_to(
            std::back_inserter(result), "a=clock-deviation:{}/{}{}", clock_deviation_->numerator,
            clock_deviation_->denominator, newline
        );
    }

    // Source filters
    for (auto& filter : source_filters_) {
        auto txt = filter.to_string();
        if (!txt) {
            return tl::make_unexpected(txt.error());
        }
        fmt::format_to(std::back_inserter(result), "{}{}", txt.value(), newline);
    }

    // Framecount (legacy RAVENNA)
    if (framecount_) {
        fmt::format_to(std::back_inserter(result), "a=framecount:{}{}", *framecount_, newline);
    }

    return result;
}

bool rav::sdp::operator==(const Format& lhs, const Format& rhs) {
    return std::tie(lhs.payload_type, lhs.encoding_name, lhs.clock_rate, lhs.num_channels)
        == std::tie(rhs.payload_type, rhs.encoding_name, rhs.clock_rate, rhs.num_channels);
}

bool rav::sdp::operator!=(const Format& lhs, const Format& rhs) {
    return !(lhs == rhs);
}
