/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/session_description.hpp"

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/log.hpp"

rav::session_description::parse_result<rav::session_description::origin_field>
rav::session_description::origin_field::parse(const std::string& line) {
    if (!starts_with(line, "o=")) {
        return result<origin_field, const char*>::err("origin: expecting 'o='");
    }

    const auto parts = split_string(line.substr(2), ' ');
    if (parts.size() != 6) {
        return parse_result<origin_field>::err("origin: expecting 6 parts");
    }

    origin_field o;

    o.username = parts[0];
    o.session_id = parts[1];

    if (const auto v = rav::from_string_strict<int>(parts[2]); v.has_value()) {
        o.session_version = *v;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse version as integer");
    }

    if (parts[3] == "IN") {
        o.network_type = netw_type::internet;
    } else {
        return parse_result<origin_field>::err("origin: invalid network type");
    }

    if (parts[4] == "IP4") {
        o.address_type = addr_type::ipv4;
    } else if (parts[4] == "IP6") {
        o.address_type = addr_type::ipv6;
    } else {
        return parse_result<origin_field>::err("origin: invalid address type");
    }

    o.unicast_address = parts[5];

    return parse_result<origin_field>::ok(std::move(o));
}

rav::result<rav::session_description::connection_info_field, const char*>
rav::session_description::connection_info_field::parse(const std::string& line) {
    if (!starts_with(line, "c=")) {
        return result<connection_info_field, const char*>::err("connection: expecting 'c='");
    }

    const auto parts = split_string(line.substr(2), ' ');
    if (parts.size() != 3) {
        return parse_result<connection_info_field>::err("connection: expecting 3 parts");
    }

    connection_info_field info;

    if (parts[0] == "IN") {
        info.network_type = netw_type::internet;
    } else {
        return parse_result<connection_info_field>::err("connection: invalid network type");
    }

    if (parts[1] == "IP4") {
        info.address_type = addr_type::ipv4;
    } else if (parts[1] == "IP6") {
        info.address_type = addr_type::ipv6;
    } else {
        return parse_result<connection_info_field>::err("connection: invalid address type");
    }

    const auto address_parts = split_string(parts[2], '/');

    RAV_ASSERT(!address_parts.empty());

    if (!address_parts.empty()) {
        info.address = address_parts[0];
    }

    if (address_parts.size() == 2) {
        if (info.address_type == addr_type::ipv4) {
            info.ttl = rav::from_string_strict<int>(address_parts[1]);
            if (!info.ttl.has_value()) {
                return parse_result<connection_info_field>::err(
                    "connection: failed to parse number of addresses as integer"
                );
            }
        } else if (info.address_type == addr_type::ipv6) {
            info.number_of_addresses = rav::from_string_strict<int>(address_parts[1]);
            if (!info.number_of_addresses.has_value()) {
                return parse_result<connection_info_field>::err(
                    "connection: failed to parse number of addresses as integer"
                );
            }
        }
    } else if (address_parts.size() == 3) {
        if (info.address_type == addr_type::ipv6) {
            return parse_result<connection_info_field>::err("connection: invalid address, ttl not allowed for ipv6");
        }
        info.ttl = rav::from_string_strict<int>(address_parts[1]);
        info.number_of_addresses = rav::from_string_strict<int>(address_parts[2]);
        if (!info.ttl.has_value()) {
            return parse_result<connection_info_field>::err("connection: failed to parse ttl as integer");
        }
        if (!info.number_of_addresses.has_value()) {
            return parse_result<connection_info_field>::err("connection: failed to parse number of addresses as integer"
            );
        }
    } else if (address_parts.size() > 3) {
        return parse_result<connection_info_field>::err("connection: invalid address, got too many forward slashes");
    }

    return parse_result<connection_info_field>::ok(std::move(info));
}

rav::session_description::parse_result<rav::session_description::time_active_field>
rav::session_description::time_active_field::parse(const std::string& line) {
    if (!starts_with(line, "t=")) {
        return parse_result<time_active_field>::err("time: expecting 't='");
    }

    time_active_field time;

    const auto parts = split_string(line.substr(2), ' ');

    if (parts.size() != 2) {
        return parse_result<time_active_field>::err("time: expecting 2 parts");
    }

    const auto start_time = rav::from_string_strict<int64_t>(parts[0]);
    if (!start_time.has_value()) {
        return parse_result<time_active_field>::err("time: failed to parse start time as integer");
    }

    const auto stop_time = rav::from_string_strict<int64_t>(parts[1]);
    if (!stop_time.has_value()) {
        return parse_result<time_active_field>::err("time: failed to parse stop time as integer");
    }

    time.start_time = *start_time;
    time.stop_time = *stop_time;

    return parse_result<time_active_field>::ok(time);
}

rav::session_description::parse_result<rav::session_description::media_description>
rav::session_description::media_description::parse(const std::string& line) {
    if (!starts_with(line, "m=")) {
        return parse_result<media_description>::err("media: expecting 'm='");
    }

    media_description media;

    const auto parts = split_string(line.substr(2), ' ');

    if (parts.size() < 4) {
        return parse_result<media_description>::err("media: expecting at least 4 parts");
    }

    media.media_type = parts[0];

    const auto port_parts = split_string(parts[1], '/');

    if (port_parts.size() > 2) {
        return parse_result<media_description>::err("media: unexpected number of parts in port");
    }

    // Port
    if (!port_parts.empty()) {
        if (const auto port = rav::from_string_strict<uint16_t>(port_parts[0]); port.has_value()) {
            media.port = *port;
        } else {
            return parse_result<media_description>::err("media: failed to parse port as integer");
        }
    }

    // Number of ports
    if (port_parts.size() == 1) {
        media.number_of_ports = 1;
    } else if (port_parts.size() == 2) {
        if (const auto num_ports = rav::from_string_strict<uint16_t>(port_parts[1]); num_ports.has_value()) {
            media.number_of_ports = *num_ports;
        } else {
            return parse_result<media_description>::err("media: failed to parse number of ports as integer");
        }
    }

    media.protocol = parts[2];

    for (size_t i = 3; i < parts.size(); ++i) {
        media.formats.push_back(parts[i]);
    }

    return parse_result<media_description>::ok(std::move(media));
}

rav::session_description::parse_result<rav::session_description>
rav::session_description::parse(const std::string& sdp_text) {
    session_description sd;
    std::istringstream stream(sdp_text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.back() == '\r') {  // Remove trailing '\r' if present (because of CRLF)
            line.pop_back();
        }

        if (line.size() < 2) {
            return parse_result<session_description>::err("session_description: line too short");
        }

        if (line[1] != '=') {
            RAV_ERROR("Invalid line: {}", line);
            return parse_result<session_description>::err("session_description: expecting equal(=) sign");
        }

        switch (line.front()) {
            case 'v': {
                auto result = parse_version(line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                sd.version_ = result.move_ok();
                break;
            }
            case 'o': {
                auto result = origin_field::parse(line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                sd.origin_ = result.move_ok();
                break;
            }
            case 's': {
                sd.session_name_ = line.substr(2);
                break;
            }
            case 'c': {
                auto result = connection_info_field::parse(line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                if (!sd.media_descriptions_.empty()) {
                    sd.media_descriptions_.back().connection_info = result.move_ok();
                } else {
                    sd.connection_info_ = result.move_ok();
                }
                break;
            }
            case 't': {
                auto result = time_active_field::parse(line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                sd.time_active_ = result.move_ok();
                break;
            }
            case 'm': {
                auto result = media_description::parse(line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                sd.media_descriptions_.push_back(result.move_ok());
                break;
            }
            default:
                continue;
        }
    }

    return parse_result<session_description>::ok(std::move(sd));
}

int rav::session_description::version() const {
    return version_;
}

const rav::session_description::origin_field& rav::session_description::origin() const {
    return origin_;
}

std::optional<rav::session_description::connection_info_field> rav::session_description::connection_info() const {
    return connection_info_;
}

std::string rav::session_description::session_name() const {
    return session_name_;
}

rav::session_description::time_active_field rav::session_description::time_active() const {
    return time_active_;
}

const std::vector<rav::session_description::media_description>& rav::session_description::media_descriptions() const {
    return media_descriptions_;
}

rav::session_description::parse_result<int> rav::session_description::parse_version(const std::string_view line) {
    if (!starts_with(line, "v=")) {
        return parse_result<int>::err("expecting line to start with 'v='");
    }

    if (const auto v = rav::from_string_strict<int>(line.substr(2)); v.has_value()) {
        if (*v != 0) {
            return parse_result<int>::err("invalid version");
        }
        return parse_result<int>::ok(*v);
    }

    return parse_result<int>::err("failed to parse integer from string");
}
