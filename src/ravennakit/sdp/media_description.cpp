/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/media_description.hpp"

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/sdp/session_description.hpp"
#include "ravennakit/sdp/source_filter.hpp"

std::string rav::sdp::format::to_string() const {
    return fmt::format("{} {}/{}/{}", payload_type, encoding_name, clock_rate, num_channels);
}

std::optional<size_t> rav::sdp::format::bytes_per_sample() const {
    if (encoding_name == "L16") {
        return 2;
    }
    if (encoding_name == "L24") {
        return 3;
    }
    if (encoding_name == "L32") {
        return 4;
    }
    return std::nullopt;
}

std::optional<size_t> rav::sdp::format::bytes_per_frame() const {
    const auto bytes_per_sample = this->bytes_per_sample();
    if (!bytes_per_sample) {
        return std::nullopt;
    }
    if (num_channels == 0) {
        RAV_WARNING("Number of channels is 0, cannot determine bytes per frame.");
        return std::nullopt;
    }
    return *bytes_per_sample * num_channels;
}

rav::sdp::format::parse_result<rav::sdp::format> rav::sdp::format::parse_new(const std::string_view line) {
    string_parser parser(line);

    format map;

    if (const auto payload_type = parser.read_int<int8_t>()) {
        map.payload_type = *payload_type;
        if (!parser.skip(' ')) {
            return parse_result<format>::err("rtpmap: expecting space after payload type");
        }
    } else {
        return parse_result<format>::err("rtpmap: invalid payload type");
    }

    if (const auto encoding_name = parser.split('/')) {
        map.encoding_name = *encoding_name;
    } else {
        return parse_result<format>::err("rtpmap: failed to parse encoding name");
    }

    if (const auto clock_rate = parser.read_int<uint32_t>()) {
        map.clock_rate = *clock_rate;
    } else {
        return parse_result<format>::err("rtpmap: invalid clock rate");
    }

    if (parser.skip('/')) {
        if (const auto num_channels = parser.read_int<uint32_t>()) {
            // Note: strictly speaking the encoding parameters can be anything, but as of now it's only used for
            // channels.
            map.num_channels = *num_channels;
        } else {
            return parse_result<format>::err("rtpmap: failed to parse number of channels");
        }
    } else {
        map.num_channels = 1;
    }

    return parse_result<format>::ok(map);
}

rav::sdp::connection_info_field::parse_result<rav::sdp::connection_info_field>
rav::sdp::connection_info_field::parse_new(const std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("c=")) {
        return parse_result<connection_info_field>::err("connection: expecting 'c='");
    }

    connection_info_field info;

    // Network type
    if (const auto network_type = parser.split(' ')) {
        if (*network_type == sdp::k_sdp_inet) {
            info.network_type = sdp::netw_type::internet;
        } else {
            return parse_result<connection_info_field>::err("connection: invalid network type");
        }
    } else {
        return parse_result<connection_info_field>::err("connection: failed to parse network type");
    }

    // Address type
    if (const auto address_type = parser.split(' ')) {
        if (*address_type == sdp::k_sdp_ipv4) {
            info.address_type = sdp::addr_type::ipv4;
        } else if (*address_type == sdp::k_sdp_ipv6) {
            info.address_type = sdp::addr_type::ipv6;
        } else {
            return parse_result<connection_info_field>::err("connection: invalid address type");
        }
    } else {
        return parse_result<connection_info_field>::err("connection: failed to parse address type");
    }

    // Address
    if (const auto address = parser.split('/')) {
        info.address = *address;
    }

    if (parser.exhausted()) {
        return parse_result<connection_info_field>::ok(std::move(info));
    }

    // Parse optional ttl and number of addresses
    if (info.address_type == sdp::addr_type::ipv4) {
        if (auto ttl = parser.read_int<int32_t>()) {
            info.ttl = *ttl;
        } else {
            return parse_result<connection_info_field>::err("connection: failed to parse ttl for ipv4 address");
        }
        if (parser.skip('/')) {
            if (auto num_addresses = parser.read_int<int32_t>()) {
                info.number_of_addresses = *num_addresses;
            } else {
                return parse_result<connection_info_field>::err(
                    "connection: failed to parse number of addresses for ipv4 address"
                );
            }
        }
    } else if (info.address_type == sdp::addr_type::ipv6) {
        if (auto num_addresses = parser.read_int<int32_t>()) {
            info.number_of_addresses = *num_addresses;
        } else {
            return parse_result<connection_info_field>::err(
                "connection: failed to parse number of addresses for ipv4 address"
            );
        }
    }

    if (!parser.exhausted()) {
        return parse_result<connection_info_field>::err("connection: unexpected characters at end of line");
    }

    return parse_result<connection_info_field>::ok(std::move(info));
}

rav::sdp::origin_field::parse_result<rav::sdp::origin_field> rav::sdp::origin_field::parse_new(std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("o=")) {
        return parse_result<origin_field>::err("origin: expecting 'o='");
    }

    origin_field o;

    // Username
    if (const auto username = parser.split(' ')) {
        o.username = *username;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse username");
    }

    // Session id
    if (const auto session_id = parser.split(' ')) {
        o.session_id = *session_id;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse session id");
    }

    // Session version
    if (const auto version = parser.read_int<int32_t>()) {
        o.session_version = *version;
        parser.skip(' ');
    } else {
        return parse_result<origin_field>::err("origin: failed to parse session version");
    }

    // Network type
    if (const auto network_type = parser.split(' ')) {
        if (*network_type != k_sdp_inet) {
            return parse_result<origin_field>::err("origin: invalid network type");
        }
        o.network_type = netw_type::internet;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse network type");
    }

    // Address type
    if (const auto address_type = parser.split(' ')) {
        if (*address_type == k_sdp_ipv4) {
            o.address_type = addr_type::ipv4;
        } else if (*address_type == k_sdp_ipv6) {
            o.address_type = addr_type::ipv6;
        } else {
            return parse_result<origin_field>::err("origin: invalid address type");
        }
    } else {
        return parse_result<origin_field>::err("origin: failed to parse address type");
    }

    // Address
    if (const auto address = parser.split(' ')) {
        o.unicast_address = *address;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse address");
    }

    return parse_result<origin_field>::ok(std::move(o));
}

rav::sdp::time_active_field::parse_result<rav::sdp::time_active_field>
rav::sdp::time_active_field::parse_new(const std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("t=")) {
        return parse_result<time_active_field>::err("time: expecting 't='");
    }

    time_active_field time;

    if (const auto start_time = parser.read_int<int64_t>()) {
        time.start_time = *start_time;
    } else {
        return parse_result<time_active_field>::err("time: failed to parse start time as integer");
    }

    if (!parser.skip(' ')) {
        return parse_result<time_active_field>::err("time: expecting space after start time");
    }

    if (const auto stop_time = parser.read_int<int64_t>()) {
        time.stop_time = *stop_time;
    } else {
        return parse_result<time_active_field>::err("time: failed to parse stop time as integer");
    }

    return parse_result<time_active_field>::ok(time);
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

rav::sdp::media_description::parse_result<rav::sdp::media_description>
rav::sdp::media_description::parse_new(const std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("m=")) {
        return parse_result<media_description>::err("media: expecting 'm='");
    }

    media_description media;

    // Media type
    if (const auto media_type = parser.split(' ')) {
        media.media_type_ = *media_type;
    } else {
        return parse_result<media_description>::err("media: failed to parse media type");
    }

    // Port
    if (const auto port = parser.read_int<uint16_t>()) {
        media.port_ = *port;
        if (parser.skip('/')) {
            if (const auto num_ports = parser.read_int<uint16_t>()) {
                media.number_of_ports_ = *num_ports;
            } else {
                return parse_result<media_description>::err("media: failed to parse number of ports as integer");
            }
        } else {
            media.number_of_ports_ = 1;
        }
        parser.skip(' ');
    } else {
        return parse_result<media_description>::err("media: failed to parse port as integer");
    }

    // Protocol
    if (const auto protocol = parser.split(' ')) {
        media.protocol_ = *protocol;
    } else {
        return parse_result<media_description>::err("media: failed to parse protocol");
    }

    // Formats
    while (const auto format_str = parser.split(' ')) {
        if (const auto value = rav::ston<int8_t>(*format_str)) {
            media.formats_.push_back({*value, {}, {}, {}});
        } else {
            return parse_result<media_description>::err("media: format integer parsing failed");
        }
    }

    return parse_result<media_description>::ok(std::move(media));
}

rav::sdp::media_description::parse_result<void>
rav::sdp::media_description::parse_attribute(const std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("a=")) {
        return parse_result<void>::err("attribute: expecting 'a='");
    }

    auto key = parser.split(':');

    if (!key) {
        return parse_result<void>::err("attribute: expecting key");
    }

    if (key == k_sdp_rtp_map) {
        if (const auto value = parser.read_until_end()) {
            auto format_result = format::parse_new(*value);
            if (format_result.is_err()) {
                return parse_result<void>::err(format_result.get_err());
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
                return parse_result<void>::err("media: rtpmap attribute for unknown payload type");
            }
        } else {
            return parse_result<void>::err("media: failed to parse rtpmap value");
        }
    } else if (key == k_sdp_ptime) {
        if (const auto value = parser.read_until_end()) {
            if (const auto ptime = rav::stod(value->data())) {
                if (*ptime < 0) {
                    return parse_result<void>::err("media: ptime must be a positive number");
                }
                ptime_ = *ptime;
            } else {
                return parse_result<void>::err("media: failed to parse ptime as double");
            }
        } else {
            return parse_result<void>::err("media: failed to parse ptime value");
        }
    } else if (key == k_sdp_max_ptime) {
        if (const auto value = parser.read_until_end()) {
            if (const auto maxptime = rav::stod(value->data())) {
                if (*maxptime < 0) {
                    return parse_result<void>::err("media: maxptime must be a positive number");
                }
                max_ptime_ = *maxptime;
            } else {
                return parse_result<void>::err("media: failed to parse ptime as double");
            }
        } else {
            return parse_result<void>::err("media: failed to parse maxptime value");
        }
    } else if (key == k_sdp_sendrecv) {
        media_direction_ = media_direction::sendrecv;
    } else if (key == k_sdp_sendonly) {
        media_direction_ = media_direction::sendonly;
    } else if (key == k_sdp_recvonly) {
        media_direction_ = media_direction::recvonly;
    } else if (key == k_sdp_inactive) {
        media_direction_ = media_direction::inactive;
    } else if (key == k_sdp_ts_refclk) {
        if (const auto value = parser.read_until_end()) {
            auto ref_clock = sdp::reference_clock::parse_new(*value);
            if (ref_clock.is_err()) {
                return parse_result<void>::err(ref_clock.get_err());
            }
            reference_clock_ = ref_clock.move_ok();
        } else {
            return parse_result<void>::err("media: failed to parse ts-refclk value");
        }
    } else if (key == sdp::media_clock_source::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock = sdp::media_clock_source::parse_new(*value);
            if (clock.is_err()) {
                return parse_result<void>::err(clock.get_err());
            }
            media_clock_ = clock.move_ok();
        } else {
            return parse_result<void>::err("media: failed to parse media clock value");
        }
    } else if (key == ravenna_clock_domain::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock_domain = ravenna_clock_domain::parse_new(*value);
            if (clock_domain.is_err()) {
                return parse_result<void>::err(clock_domain.get_err());
            }
            clock_domain_ = clock_domain.move_ok();
        } else {
            return parse_result<void>::err("media: failed to parse clock domain value");
        }
    } else if (key == "sync-time") {
        if (const auto rtp_ts = parser.read_int<uint32_t>()) {
            sync_time_ = *rtp_ts;
        } else {
            return parse_result<void>::err("media: failed to parse sync-time value");
        }
    } else if (key == "clock-deviation") {
        const auto num = parser.read_int<uint32_t>();
        if (!num) {
            return parse_result<void>::err("media: failed to parse clock-deviation value");
        }
        if (!parser.skip('/')) {
            return parse_result<void>::err("media: expecting '/' after clock-deviation numerator value");
        }
        const auto denom = parser.read_int<uint32_t>();
        if (!denom) {
            return parse_result<void>::err("media: failed to parse clock-deviation denominator value");
        }
        clock_deviation_ = fraction<uint32_t> {*num, *denom};
    } else if (key == source_filter::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto filter = source_filter::parse_new(*value);
            if (filter.is_err()) {
                return parse_result<void>::err(filter.get_err());
            }
            source_filters_.push_back(filter.move_ok());
        } else {
            return parse_result<void>::err("media: failed to parse source-filter value");
        }
    } else if (key == "framecount") {
        if (auto value = parser.read_int<int>()) {
            framecount_ = *value;
        } else {
            return parse_result<void>::err("media: failed to parse framecount value");
        }
    } else {
        // Store the attribute in the map of unknown attributes
        if (auto value = parser.read_until_end()) {
            attributes_.emplace(*key, *value);
        } else {
            return parse_result<void>::err("media: failed to parse attribute value");
        }
    }

    return parse_result<void>::ok();
}

const std::string& rav::sdp::media_description::media_type() const {
    return media_type_;
}

uint16_t rav::sdp::media_description::port() const {
    return port_;
}

uint16_t rav::sdp::media_description::number_of_ports() const {
    return number_of_ports_;
}

const std::string& rav::sdp::media_description::protocol() const {
    return protocol_;
}

const std::vector<rav::sdp::format>& rav::sdp::media_description::formats() const {
    return formats_;
}

const std::vector<rav::sdp::connection_info_field>& rav::sdp::media_description::connection_infos() const {
    return connection_infos_;
}

void rav::sdp::media_description::add_connection_info(connection_info_field connection_info) {
    connection_infos_.push_back(std::move(connection_info));
}

void rav::sdp::media_description::set_session_information(std::string session_information) {
    session_information_ = std::move(session_information);
}

std::optional<double> rav::sdp::media_description::ptime() const {
    return ptime_;
}

std::optional<double> rav::sdp::media_description::max_ptime() const {
    return max_ptime_;
}

const std::optional<rav::sdp::media_direction>& rav::sdp::media_description::direction() const {
    return media_direction_;
}

const std::optional<rav::sdp::reference_clock>& rav::sdp::media_description::ref_clock() const {
    return reference_clock_;
}

const std::optional<rav::sdp::media_clock_source>& rav::sdp::media_description::media_clock() const {
    return media_clock_;
}

const std::optional<std::string>& rav::sdp::media_description::session_information() const {
    return session_information_;
}

std::optional<uint32_t> rav::sdp::media_description::sync_time() const {
    return sync_time_;
}

const std::optional<rav::fraction<unsigned>>& rav::sdp::media_description::clock_deviation() const {
    return clock_deviation_;
}

const std::vector<rav::sdp::source_filter>& rav::sdp::media_description::source_filters() const {
    return source_filters_;
}

std::optional<int> rav::sdp::media_description::framecount() const {
    return framecount_;
}

const std::map<std::string, std::string>& rav::sdp::media_description::attributes() const {
    return attributes_;
}

bool rav::sdp::operator==(const format& lhs, const format& rhs) {
    return std::tie(lhs.payload_type, lhs.encoding_name, lhs.clock_rate, lhs.num_channels)
        == std::tie(rhs.payload_type, rhs.encoding_name, rhs.clock_rate, rhs.num_channels);
}

bool rav::sdp::operator!=(const format& lhs, const format& rhs) {
    return !(lhs == rhs);
}
