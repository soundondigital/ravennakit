/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/sdp_session_description.hpp"

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/exception.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/sdp/sdp_media_description.hpp"
#include "ravennakit/sdp/detail/sdp_constants.hpp"
#include "ravennakit/sdp/detail/sdp_reference_clock.hpp"

void rav::sdp::SessionDescription::add_or_update_source_filter(const SourceFilter& filter) {
    for (auto& f : source_filters) {
        if (f.net_type == filter.net_type && f.addr_type == filter.addr_type && f.dest_address == filter.dest_address) {
            f = filter;
            return;
        }
    }

    source_filters.push_back(filter);
}

tl::expected<void, std::string> rav::sdp::SessionDescription::parse_attribute(const std::string_view line) {
    StringParser parser(line);

    if (!parser.skip("a=")) {
        return tl::unexpected("attribute: expecting 'a='");
    }

    const auto key = parser.split(':');

    if (!key) {
        return tl::unexpected("attribute: expecting key");
    }

    if (key == k_sdp_sendrecv) {
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
        }
    } else if (key == MediaClockSource::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock = parse_media_clock_source(*value);
            if (!clock) {
                return tl::unexpected(clock.error());
            }
            media_clock = *clock;
        }
    } else if (key == RavennaClockDomain::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock_domain = parse_ravenna_clock_domain(*value);
            if (!clock_domain) {
                return tl::unexpected(clock_domain.error());
            }
            ravenna_clock_domain = *clock_domain;
        }
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
    } else if (key == k_sdp_sync_time) {
        if (const auto rtp_ts = parser.read_int<uint32_t>()) {
            ravenna_sync_time = *rtp_ts;
        } else {
            return tl::unexpected("media: failed to parse sync-time value");
        }
    } else if (key == k_sdp_group) {
        if (const auto value = parser.read_until_end()) {
            auto new_group= parse_group(*value);
            if (!new_group.has_value()) {
                return tl::unexpected(new_group.error());
            }
            group = *new_group;
        }
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

tl::expected<int, std::string> rav::sdp::parse_version(const std::string_view line) {
    if (!string_starts_with(line, "v=")) {
        return tl::unexpected("expecting line to start with 'v='");
    }

    if (const auto v = rav::string_to_int<int>(line.substr(2)); v.has_value()) {
        if (*v != 0) {
            return tl::unexpected("invalid version");
        }
        return *v;
    }

    return tl::unexpected("failed to parse integer from string");
}

tl::expected<rav::sdp::SessionDescription, std::string>
rav::sdp::parse_session_description(const std::string& sdp_text) {
    SessionDescription sd;
    StringParser parser(sdp_text);

    for (auto line = parser.read_line(); line.has_value(); line = parser.read_line()) {
        if (line->empty()) {
            continue;
        }

        switch (line->front()) {
            case 'v': {
                auto result = parse_version(*line);
                if (!result) {
                    return tl::unexpected(result.error());
                }
                sd.version = *result;
                break;
            }
            case 'o': {
                auto result = parse_origin(*line);
                if (!result) {
                    return tl::unexpected(result.error());
                }
                sd.origin = std::move(*result);
                break;
            }
            case 's': {
                sd.session_name = line->substr(2);
                break;
            }
            case 'c': {
                auto result = parse_connection_info(*line);
                if (!result) {
                    return tl::unexpected(result.error());
                }
                if (!sd.media_descriptions.empty()) {
                    sd.media_descriptions.back().connection_infos.push_back(std::move(*result));
                } else {
                    sd.connection_info = std::move(*result);
                }
                break;
            }
            case 't': {
                auto time_active = parse_time_active(*line);
                if (!time_active) {
                    return tl::unexpected(time_active.error());
                }
                sd.time_active = *time_active;
                break;
            }
            case 'm': {
                auto desc = parse_media_description(*line);
                if (!desc) {
                    return tl::unexpected(desc.error());
                }
                sd.media_descriptions.push_back(*desc);
                break;
            }
            case 'a': {
                if (!sd.media_descriptions.empty()) {
                    auto result = sd.media_descriptions.back().parse_attribute(*line);
                    if (!result) {
                        return tl::unexpected(result.error());
                    }
                } else {
                    auto result = sd.parse_attribute(*line);
                    if (!result) {
                        return tl::unexpected(result.error());
                    }
                }
                break;
            }
            case 'i': {
                if (!sd.media_descriptions.empty()) {
                    sd.media_descriptions.back().session_information = std::string(line->substr(2));
                } else {
                    sd.session_information = line->substr(2);
                }
                break;
            }
            default:
                return tl::unexpected(fmt::format("Unknown line: {}", *line));
        }
    }

    return sd;
}

std::optional<std::string> rav::sdp::to_string(const SessionDescription& session_description, const char* newline) {
    RAV_ASSERT(newline != nullptr, "newline must not be nullptr");

    if (!validate(session_description.origin)) {
        return std::nullopt;
    }

    std::string sdp;

    // Version
    fmt::format_to(std::back_inserter(sdp), "v={}{}", session_description.version, newline);

    // Origin
    fmt::format_to(std::back_inserter(sdp), "{}{}", to_string(session_description.origin), newline);

    // Session name
    fmt::format_to(
        std::back_inserter(sdp), "s={}{}",
        session_description.session_name.empty() ? "-" : session_description.session_name, newline
    );

    // Time active
    fmt::format_to(std::back_inserter(sdp), "{}{}", to_string(session_description.time_active), newline);

    // Group duplication
    if (session_description.group.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", to_string(*session_description.group), newline);
    }

    // Connection info
    if (session_description.connection_info.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", to_string(*session_description.connection_info), newline);
    }

    // Clock domain
    if (session_description.ravenna_clock_domain.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", to_string(*session_description.ravenna_clock_domain), newline);
    }

    // Ref clock
    if (session_description.reference_clock.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", to_string(*session_description.reference_clock), newline);
    }

    // Media direction
    if (session_description.media_direction.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "a={}{}", to_string(*session_description.media_direction), newline);
    }

    // Media clock source
    if (session_description.media_clock.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", to_string(*session_description.media_clock), newline);
    }

    // Source filters
    for (auto& filter : session_description.source_filters) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", to_string(filter), newline);
    }

    // Media descriptions
    for (const auto& media : session_description.media_descriptions) {
        fmt::format_to(std::back_inserter(sdp), "{}", to_string(media));
    }

    return sdp;
}
