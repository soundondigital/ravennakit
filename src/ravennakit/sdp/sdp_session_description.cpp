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
#include "ravennakit/core/todo.hpp"
#include "ravennakit/sdp/sdp_media_description.hpp"
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

const std::optional<rav::sdp::connection_info_field>& rav::sdp::session_description::connection_info() const {
    return connection_info_;
}

const std::string& rav::sdp::session_description::session_name() const {
    return session_name_;
}

rav::sdp::time_active_field rav::sdp::session_description::time_active() const {
    return time_active_;
}

const std::vector<rav::sdp::media_description>& rav::sdp::session_description::media_descriptions() const {
    return media_descriptions_;
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

const std::optional<rav::sdp::media_clock_source>& rav::sdp::session_description::media_clock() const {
    return media_clock_;
}

const std::optional<rav::sdp::ravenna_clock_domain>& rav::sdp::session_description::clock_domain() const {
    return clock_domain_;
}

const std::vector<rav::sdp::source_filter>& rav::sdp::session_description::source_filters() const {
    return source_filters_;
}

const std::map<std::string, std::string>& rav::sdp::session_description::attributes() const {
    return attributes_;
}

std::string rav::sdp::session_description::to_string() const {
    std::string sdp;
    sdp += fmt::format("v={}\n", version_);
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

std::string rav::sdp::to_string(const filter_mode& filter_mode) {
    switch (filter_mode) {
        case filter_mode::exclude:
            return "excl";
        case filter_mode::include:
            return "incl";
        case filter_mode::undefined:
            return "undefined";
        default:
            return "undef";
    }
}
