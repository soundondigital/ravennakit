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

rav::sdp::session_description::parse_result<rav::sdp::session_description>
rav::sdp::session_description::parse_new(const std::string& sdp_text) {
    session_description sd;
    string_parser parser(sdp_text);

    for (auto line = parser.read_line(); line.has_value(); line = parser.read_line()) {
        if (line->empty()) {
            continue;
        }

        switch (line->front()) {
            case 'v': {
                auto result = parse_version(*line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                sd.version_ = result.move_ok();
                break;
            }
            case 'o': {
                auto result = sdp::origin_field::parse_new(*line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                sd.origin_ = result.move_ok();
                break;
            }
            case 's': {
                sd.session_name_ = line->substr(2);
                break;
            }
            case 'c': {
                auto result = sdp::connection_info_field::parse_new(*line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                if (!sd.media_descriptions_.empty()) {
                    sd.media_descriptions_.back().add_connection_info(result.move_ok());
                } else {
                    sd.connection_info_ = result.move_ok();
                }
                break;
            }
            case 't': {
                auto result = sdp::time_active_field::parse_new(*line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                sd.time_active_ = result.move_ok();
                break;
            }
            case 'm': {
                auto result = sdp::media_description::parse_new(*line);
                if (result.is_err()) {
                    return parse_result<session_description>::err(result.get_err());
                }
                sd.media_descriptions_.push_back(result.move_ok());
                break;
            }
            case 'a': {
                if (!sd.media_descriptions_.empty()) {
                    auto result = sd.media_descriptions_.back().parse_attribute(*line);
                    if (result.is_err()) {
                        return parse_result<session_description>::err(result.get_err());
                    }
                } else {
                    auto result = sd.parse_attribute(*line);
                    if (result.is_err()) {
                        return parse_result<session_description>::err(result.get_err());
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
                continue;
        }
    }

    return parse_result<session_description>::ok(std::move(sd));
}

int rav::sdp::session_description::version() const {
    return version_;
}

const rav::sdp::origin_field& rav::sdp::session_description::origin() const {
    return origin_;
}

void rav::sdp::session_description::set_origin(origin_field origin) {
    origin_ = std::move(origin);
}

const std::optional<rav::sdp::connection_info_field>& rav::sdp::session_description::connection_info() const {
    return connection_info_;
}

void rav::sdp::session_description::set_connection_info(connection_info_field connection_info) {
    connection_info_ = std::move(connection_info);
}

const std::string& rav::sdp::session_description::session_name() const {
    return session_name_;
}

void rav::sdp::session_description::set_session_name(std::string session_name) {
    session_name_ = std::move(session_name);
}

rav::sdp::time_active_field rav::sdp::session_description::time_active() const {
    return time_active_;
}

void rav::sdp::session_description::set_time_active(const time_active_field time_active) {
    time_active_ = time_active;
}

const std::vector<rav::sdp::media_description>& rav::sdp::session_description::media_descriptions() const {
    return media_descriptions_;
}

void rav::sdp::session_description::add_media_description(media_description media_description) {
    media_descriptions_.push_back(std::move(media_description));
}

rav::sdp::media_direction rav::sdp::session_description::direction() const {
    if (media_direction_.has_value()) {
        return *media_direction_;
    }
    return sdp::media_direction::sendrecv;
}

std::optional<rav::sdp::reference_clock> rav::sdp::session_description::ref_clock() const {
    return reference_clock_;
}

void rav::sdp::session_description::set_ref_clock(reference_clock ref_clock) {
    reference_clock_ = std::move(ref_clock);
}

const std::optional<rav::sdp::media_clock_source>& rav::sdp::session_description::media_clock() const {
    return media_clock_;
}

void rav::sdp::session_description::set_media_clock(media_clock_source media_clock) {
    media_clock_ = std::move(media_clock);
}

const std::optional<rav::sdp::ravenna_clock_domain>& rav::sdp::session_description::clock_domain() const {
    return clock_domain_;
}

void rav::sdp::session_description::set_clock_domain(ravenna_clock_domain clock_domain) {
    clock_domain_ = clock_domain;
}

const std::vector<rav::sdp::source_filter>& rav::sdp::session_description::source_filters() const {
    return source_filters_;
}

void rav::sdp::session_description::add_source_filter(const source_filter& filter) {
    for (auto& f : source_filters_) {
        if (f.network_type() == filter.network_type() && f.address_type() == filter.address_type()
            && f.dest_address() == filter.dest_address()) {
            f = filter;
            return;
        }
    }

    source_filters_.push_back(filter);
}

std::optional<rav::sdp::media_direction> rav::sdp::session_description::get_media_direction() const {
    return media_direction_;
}

void rav::sdp::session_description::set_media_direction(media_direction direction) {
    media_direction_ = direction;
}

const std::map<std::string, std::string>& rav::sdp::session_description::attributes() const {
    return attributes_;
}

tl::expected<std::string, std::string> rav::sdp::session_description::to_string(const char* newline) const {
    RAV_ASSERT(newline != nullptr, "newline must not be nullptr");
    std::string sdp;

    // Version
    fmt::format_to(std::back_inserter(sdp), "v={}{}", version_, newline);

    // Origin
    auto origin = origin_.to_string();
    if (!origin) {
        return origin;
    }
    fmt::format_to(std::back_inserter(sdp), "{}{}", origin.value(), newline);

    // Session name
    fmt::format_to(std::back_inserter(sdp), "s={}{}", session_name_.empty() ? "-" : session_name(), newline);

    // Time active
    auto time = time_active_.to_string();
    if (!time) {
        return time;
    }
    fmt::format_to(std::back_inserter(sdp), "{}{}", time.value(), newline);

    // Connection info
    if (connection_info_.has_value()) {
        auto connection = connection_info_->to_string();
        if (!connection) {
            return connection;
        }
        fmt::format_to(std::back_inserter(sdp), "{}{}", connection.value(), newline);
    }

    // Clock domain
    if (clock_domain().has_value()) {
        auto clock = clock_domain_->to_string();
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
        auto clock = media_clock_->to_string();
        if (!clock) {
            return clock;
        }
        fmt::format_to(std::back_inserter(sdp), "{}{}", clock.value(), newline);
    }

    // Source filters
    for (auto& filter : source_filters_) {
        auto source_filter = filter.to_string();
        if (!source_filter) {
            return source_filter;
        }
        fmt::format_to(std::back_inserter(sdp), "{}{}", source_filter.value(), newline);
    }

    for (const auto& media : media_descriptions_) {
        auto media_str = media.to_string(newline);
        if (!media_str) {
            return media_str;
        }
        fmt::format_to(std::back_inserter(sdp), "{}", media_str.value());
    }

    return sdp;
}

rav::sdp::session_description::parse_result<int>
rav::sdp::session_description::parse_version(const std::string_view line) {
    if (!string_starts_with(line, "v=")) {
        return parse_result<int>::err("expecting line to start with 'v='");
    }

    if (const auto v = rav::ston<int>(line.substr(2)); v.has_value()) {
        if (*v != 0) {
            return parse_result<int>::err("invalid version");
        }
        return parse_result<int>::ok(*v);
    }

    return parse_result<int>::err("failed to parse integer from string");
}

rav::sdp::session_description::parse_result<void>
rav::sdp::session_description::parse_attribute(const std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("a=")) {
        return parse_result<void>::err("attribute: expecting 'a='");
    }

    const auto key = parser.split(':');

    if (!key) {
        return parse_result<void>::err("attribute: expecting key");
    }

    if (key == sdp::k_sdp_sendrecv) {
        media_direction_ = sdp::media_direction::sendrecv;
    } else if (key == sdp::k_sdp_sendonly) {
        media_direction_ = sdp::media_direction::sendonly;
    } else if (key == sdp::k_sdp_recvonly) {
        media_direction_ = sdp::media_direction::recvonly;
    } else if (key == sdp::k_sdp_inactive) {
        media_direction_ = sdp::media_direction::inactive;
    } else if (key == sdp::k_sdp_ts_refclk) {
        if (const auto value = parser.read_until_end()) {
            auto ref_clock = sdp::reference_clock::parse_new(*value);
            if (ref_clock.is_err()) {
                return parse_result<void>::err(ref_clock.get_err());
            }
            reference_clock_ = ref_clock.move_ok();
        }
    } else if (key == sdp::media_clock_source::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock = sdp::media_clock_source::parse_new(*value);
            if (clock.is_err()) {
                return parse_result<void>::err(clock.get_err());
            }
            media_clock_ = clock.move_ok();
        }
    } else if (key == ravenna_clock_domain::k_attribute_name) {
        if (const auto value = parser.read_until_end()) {
            auto clock_domain = ravenna_clock_domain::parse_new(*value);
            if (clock_domain.is_err()) {
                return parse_result<void>::err(clock_domain.get_err());
            }
            clock_domain_ = clock_domain.move_ok();
        }
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
