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

tl::expected<rav::sdp::SessionDescription, std::string>
rav::sdp::SessionDescription::parse_new(const std::string& sdp_text) {
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
                sd.version_ = *result;
                break;
            }
            case 'o': {
                auto result = parse_origin(*line);
                if (!result) {
                    return tl::unexpected(result.error());
                }
                sd.origin_ = std::move(*result);
                break;
            }
            case 's': {
                sd.session_name_ = line->substr(2);
                break;
            }
            case 'c': {
                auto result = parse_connection_info(*line);
                if (!result) {
                    return tl::unexpected(result.error());
                }
                if (!sd.media_descriptions_.empty()) {
                    sd.media_descriptions_.back().add_connection_info(std::move(*result));
                } else {
                    sd.connection_info_ = std::move(*result);
                }
                break;
            }
            case 't': {
                auto time_active = parse_time_active(*line);
                if (!time_active) {
                    return tl::unexpected(time_active.error());
                }
                sd.time_active_ = *time_active;
                break;
            }
            case 'm': {
                auto desc = MediaDescription::parse_new(*line);
                if (!desc) {
                    return tl::unexpected(desc.error());
                }
                sd.media_descriptions_.push_back(*desc);
                break;
            }
            case 'a': {
                if (!sd.media_descriptions_.empty()) {
                    auto result = sd.media_descriptions_.back().parse_attribute(*line);
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
                if (!sd.media_descriptions_.empty()) {
                    sd.media_descriptions_.back().set_session_information(std::string(line->substr(2)));
                } else {
                    sd.session_information_ = line->substr(2);
                }
                break;
            }
            default:
                return tl::unexpected(fmt::format("Unknown line: {}", *line));
        }
    }

    return sd;
}

int rav::sdp::SessionDescription::version() const {
    return version_;
}

const rav::sdp::OriginField& rav::sdp::SessionDescription::origin() const {
    return origin_;
}

void rav::sdp::SessionDescription::set_origin(OriginField origin) {
    origin_ = std::move(origin);
}

const std::optional<rav::sdp::ConnectionInfoField>& rav::sdp::SessionDescription::connection_info() const {
    return connection_info_;
}

void rav::sdp::SessionDescription::set_connection_info(ConnectionInfoField connection_info) {
    connection_info_ = std::move(connection_info);
}

const std::string& rav::sdp::SessionDescription::session_name() const {
    return session_name_;
}

void rav::sdp::SessionDescription::set_session_name(std::string session_name) {
    session_name_ = std::move(session_name);
}

rav::sdp::TimeActiveField rav::sdp::SessionDescription::time_active() const {
    return time_active_;
}

void rav::sdp::SessionDescription::set_time_active(const TimeActiveField time_active) {
    time_active_ = time_active;
}

const std::vector<rav::sdp::MediaDescription>& rav::sdp::SessionDescription::media_descriptions() const {
    return media_descriptions_;
}

void rav::sdp::SessionDescription::add_media_description(MediaDescription media_description) {
    media_descriptions_.push_back(std::move(media_description));
}

rav::sdp::MediaDirection rav::sdp::SessionDescription::direction() const {
    if (media_direction_.has_value()) {
        return *media_direction_;
    }
    return MediaDirection::sendrecv;
}

std::optional<rav::sdp::ReferenceClock> rav::sdp::SessionDescription::ref_clock() const {
    return reference_clock_;
}

void rav::sdp::SessionDescription::set_ref_clock(ReferenceClock ref_clock) {
    reference_clock_ = std::move(ref_clock);
}

const std::optional<rav::sdp::MediaClockSource>& rav::sdp::SessionDescription::media_clock() const {
    return media_clock_;
}

void rav::sdp::SessionDescription::set_media_clock(MediaClockSource media_clock) {
    media_clock_ = std::move(media_clock);
}

const std::optional<rav::sdp::RavennaClockDomain>& rav::sdp::SessionDescription::clock_domain() const {
    return ravenna_clock_domain_;
}

void rav::sdp::SessionDescription::set_clock_domain(RavennaClockDomain clock_domain) {
    ravenna_clock_domain_ = clock_domain;
}

const std::vector<rav::sdp::SourceFilter>& rav::sdp::SessionDescription::source_filters() const {
    return source_filters_;
}

void rav::sdp::SessionDescription::add_source_filter(const SourceFilter& filter) {
    for (auto& f : source_filters_) {
        if (f.network_type() == filter.network_type() && f.address_type() == filter.address_type()
            && f.dest_address() == filter.dest_address()) {
            f = filter;
            return;
        }
    }

    source_filters_.push_back(filter);
}

std::optional<rav::sdp::MediaDirection> rav::sdp::SessionDescription::get_media_direction() const {
    return media_direction_;
}

void rav::sdp::SessionDescription::set_media_direction(MediaDirection direction) {
    media_direction_ = direction;
}

std::optional<uint32_t> rav::sdp::SessionDescription::sync_time() const {
    return sync_time_;
}

void rav::sdp::SessionDescription::set_sync_time(const std::optional<uint32_t> sync_time) {
    sync_time_ = sync_time;
}

std::optional<rav::sdp::Group> rav::sdp::SessionDescription::get_group() const {
    return group_;
}

void rav::sdp::SessionDescription::set_group(Group group) {
    group_ = std::move(group);
}

const std::map<std::string, std::string>& rav::sdp::SessionDescription::attributes() const {
    return attributes_;
}

tl::expected<std::string, std::string> rav::sdp::SessionDescription::to_string(const char* newline) const {
    RAV_ASSERT(newline != nullptr, "newline must not be nullptr");
    std::string sdp;

    // Version
    fmt::format_to(std::back_inserter(sdp), "v={}{}", version_, newline);

    // Origin
    fmt::format_to(std::back_inserter(sdp), "{}{}", sdp::to_string(origin_), newline);

    // Session name
    fmt::format_to(std::back_inserter(sdp), "s={}{}", session_name_.empty() ? "-" : session_name(), newline);

    // Time active
    fmt::format_to(std::back_inserter(sdp), "{}{}", sdp::to_string(time_active_), newline);

    // Group duplication
    if (group_.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", sdp::to_string(*group_), newline);
    }

    // Connection info
    if (connection_info_.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", sdp::to_string(*connection_info_), newline);
    }

    // Clock domain
    if (clock_domain().has_value()) {
        auto clock = sdp::to_string(*ravenna_clock_domain_);
        if (!clock) {
            return clock;
        }
        fmt::format_to(std::back_inserter(sdp), "{}{}", clock.value(), newline);
    }

    // Ref clock
    if (reference_clock_.has_value()) {
        auto ref_clock = reference_clock_->to_string();
        if (!ref_clock) {
            return ref_clock;
        }
        fmt::format_to(std::back_inserter(sdp), "{}{}", ref_clock.value(), newline);
    }

    // Media direction
    if (media_direction_.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "a={}{}", sdp::to_string(*media_direction_), newline);
    }

    // Media clock source
    if (media_clock_.has_value()) {
        fmt::format_to(std::back_inserter(sdp), "{}{}", sdp::to_string(*media_clock_), newline);
    }

    // Source filters
    for (auto& filter : source_filters_) {
        auto source_filter = filter.to_string();
        if (!source_filter) {
            return source_filter;
        }
        fmt::format_to(std::back_inserter(sdp), "{}{}", source_filter.value(), newline);
    }

    // Media descriptions
    for (const auto& media : media_descriptions_) {
        auto media_str = media.to_string(newline);
        if (!media_str) {
            return media_str;
        }
        fmt::format_to(std::back_inserter(sdp), "{}", media_str.value());
    }

    return sdp;
}

tl::expected<int, std::string> rav::sdp::SessionDescription::parse_version(const std::string_view line) {
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
        media_direction_ = MediaDirection::sendrecv;
    } else if (key == k_sdp_sendonly) {
        media_direction_ = MediaDirection::sendonly;
    } else if (key == k_sdp_recvonly) {
        media_direction_ = MediaDirection::recvonly;
    } else if (key == k_sdp_inactive) {
        media_direction_ = MediaDirection::inactive;
    } else if (key == k_sdp_ts_refclk) {
        if (const auto value = parser.read_until_end()) {
            auto ref_clock = ReferenceClock::parse_new(*value);
            if (!ref_clock) {
                return tl::unexpected(ref_clock.error());
            }
            reference_clock_ = std::move(*ref_clock);
        }
    } else if (key == MediaClockSource::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock = parse_media_clock_source(*value);
            if (!clock) {
                return tl::unexpected(clock.error());
            }
            media_clock_ = *clock;
        }
    } else if (key == RavennaClockDomain::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock_domain = parse_ravenna_clock_domain(*value);
            if (!clock_domain) {
                return tl::unexpected(clock_domain.error());
            }
            ravenna_clock_domain_ = *clock_domain;
        }
    } else if (key == SourceFilter::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto filter = SourceFilter::parse_new(*value);
            if (!filter) {
                return tl::unexpected(filter.error());
            }
            source_filters_.push_back(std::move(*filter));
        } else {
            return tl::unexpected("media: failed to parse source-filter value");
        }
    } else if (key == k_sdp_sync_time) {
        if (const auto rtp_ts = parser.read_int<uint32_t>()) {
            sync_time_ = *rtp_ts;
        } else {
            return tl::unexpected("media: failed to parse sync-time value");
        }
    } else if (key == k_sdp_group) {
        if (const auto value = parser.read_until_end()) {
            auto group = parse_group(*value);
            if (!group.has_value()) {
                return tl::unexpected(group.error());
            }
            group_ = *group;
        }
    } else {
        // Store the attribute in the map of unknown attributes
        if (auto value = parser.read_until_end()) {
            attributes_.emplace(*key, *value);
        } else {
            return tl::unexpected("media: failed to parse attribute value");
        }
    }

    return {};
}
