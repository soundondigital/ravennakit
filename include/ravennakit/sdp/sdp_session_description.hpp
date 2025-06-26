/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "detail/sdp_constants.hpp"
#include "detail/sdp_group.hpp"
#include "detail/sdp_origin.hpp"
#include "detail/sdp_reference_clock.hpp"
#include "detail/sdp_time_active.hpp"
#include "sdp_media_description.hpp"

#include <vector>

namespace rav::sdp {

/**
 * A struct that represents an SDP session description as defined in RFC 8866.
 * https://datatracker.ietf.org/doc/html/rfc8866
 */
struct SessionDescription {
    /// Type to specify which section of the SDP we are parsing
    enum class Section { session_description, media_description };

    int version {};
    OriginField origin;
    std::string session_name;
    std::optional<ConnectionInfoField> connection_info;
    TimeActiveField time_active;
    std::optional<std::string> session_information;
    std::optional<MediaDirection> media_direction;
    std::optional<ReferenceClock> reference_clock;
    std::optional<MediaClockSource> media_clock;
    std::optional<RavennaClockDomain> ravenna_clock_domain;  // RAVENNA-specific attribute
    std::optional<uint32_t> ravenna_sync_time;               // RAVENNA-specific attribute
    std::vector<SourceFilter> source_filters;
    std::optional<Group> group;
    std::map<std::string, std::string> attributes;  // Remaining, unknown attributes
    std::vector<MediaDescription> media_descriptions;

    /**
     * Adds a source filter to the session description. If the filter already exists, it will be replaced.
     * @param filter The source filter to add.
     */
    void add_or_update_source_filter(const SourceFilter& filter);

    /**
     * Parse an attribute from given line and adds it to attributes.
     * @param line The input text.
     * @return An expected indicating success or an error.
     */
    tl::expected<void, std::string> parse_attribute(std::string_view line);
};

[[nodiscard]] tl::expected<int, std::string> parse_version(std::string_view line);

/**
 * Parses an SDP session description from a string.
 * @param sdp_text The SDP text to parse.
 * @return A result indicating whether the parsing was successful or not. The error will be a message explaining
 * what went wrong.
 */
[[nodiscard]] tl::expected<SessionDescription, std::string> parse_session_description(const std::string& sdp_text);

/**
 * Converts the session description to a string.
 * @return The session description as a string.
 */
[[nodiscard]] std::string to_string(const SessionDescription& session_description, const char* newline = k_sdp_crlf);

}  // namespace rav::sdp
