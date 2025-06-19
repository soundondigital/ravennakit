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
 * A class that represents an SDP session description as defined in RFC 8866.
 * https://datatracker.ietf.org/doc/html/rfc8866
 */
class SessionDescription {
  public:
    /**
     * Parses an SDP session description from a string.
     * @param sdp_text The SDP text to parse.
     * @return A result indicating whether the parsing was successful or not. The error will be a message explaining
     * what went wrong.
     */
    static tl::expected<SessionDescription, std::string> parse_new(const std::string& sdp_text);

    /**
     * @returns The version of the SDP session description.
     */
    [[nodiscard]] int version() const;

    /**
     * @returns The origin of the SDP session description.
     */
    [[nodiscard]] const OriginField& origin() const;

    /**
     * Sets the origin of the SDP session description.
     * @param origin The origin to set.
     */
    void set_origin(OriginField origin);

    /**
     * @return The connection information of the SDP session description.
     */
    [[nodiscard]] const std::optional<ConnectionInfoField>& connection_info() const;

    /**
     * Sets the connection information of the SDP session description.
     * @param connection_info The connection information to set.
     */
    void set_connection_info(ConnectionInfoField connection_info);

    /**
     * @returns The session name of the SDP session description.
     */
    [[nodiscard]] const std::string& session_name() const;

    /**
     * Sets the session name of the SDP session description.
     * @param session_name The session name to set.
     */
    void set_session_name(std::string session_name);

    /**
     * @return The time field of the SDP session description.
     */
    [[nodiscard]] TimeActiveField time_active() const;

    /**
     * Sets the time field of the SDP session description.
     * @param time_active The time field to set.
     */
    void set_time_active(TimeActiveField time_active);

    /**
     * @returns The media descriptions of the SDP session description.
     */
    [[nodiscard]] const std::vector<MediaDescription>& media_descriptions() const;

    /**
     * Adds a media description to the SDP session description.
     * @param media_description The media description to add.
     */
    void add_media_description(MediaDescription media_description);

    /**
     * @return The direction of the media description. If the direction is not specified, the return value is sendrecv
     * which is the default as specified in RFC 8866 section 6.7).
     */
    [[nodiscard]] MediaDirection direction() const;

    /**
     * @return The reference clock of the session description.
     */
    [[nodiscard]] std::optional<ReferenceClock> ref_clock() const;

    /**
     * Sets the reference clock of the session description.
     * @param ref_clock The reference clock to set.
     */
    void set_ref_clock(ReferenceClock ref_clock);

    /**
     * @return The media clock of the session description.
     */
    [[nodiscard]] const std::optional<MediaClockSource>& media_clock() const;

    /**
     * Sets the media clock of the session description.
     * @param media_clock The media clock to set.
     */
    void set_media_clock(MediaClockSource media_clock);

    /**
     * @return The clock domain of the session description. This is a RAVENNA-specific attribute extension.
     */
    [[nodiscard]] const std::optional<RavennaClockDomain>& clock_domain() const;

    /**
     * Sets the clock domain of the session description.
     * @param clock_domain The clock domain to set.
     */
    void set_clock_domain(RavennaClockDomain clock_domain);

    /**
     * @returns The source filters of the media description.
     */
    [[nodiscard]] const std::vector<SourceFilter>& source_filters() const;

    /**
     * Adds a source filter to the session description. If the filter already exists, it will be replaced.
     * @param filter The source filter to add.
     */
    void add_source_filter(const SourceFilter& filter);

    /**
     * @return The media direction of the session description.
     */
    [[nodiscard]] std::optional<MediaDirection> get_media_direction() const;

    /**
     * Sets the media direction of the session description.
     * @param direction The media direction to set.
     */
    void set_media_direction(MediaDirection direction);

    /**
     * @return The sync-time of the stream. This is a RAVENNA-specific attribute extension and redundant to
     * mediaclk:direct.
     */
    [[nodiscard]] std::optional<uint32_t> sync_time() const;

    /**
     * Sets the sync-time of the stream. This is a RAVENNA-specific attribute extension and redundant to
     * mediaclk:direct.
     * @param sync_time The sync-time to set.
     */
    void set_sync_time(std::optional<uint32_t> sync_time);

    /**
     * @return Group attribute of the session description.
     */
    [[nodiscard]] std::optional<Group> get_group() const;

    /**
     * @param group The group to set.
     */
    void set_group(Group group);

    /**
     * @returns Attributes which have not been parsed into a specific field.
     */
    [[nodiscard]] const std::map<std::string, std::string>& attributes() const;

    /**
     * Converts the session description to a string.
     * @return The session description as a string.
     * TODO: Always return a string without validating.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string(const char* newline = k_sdp_crlf) const;

  private:
    /// Type to specify which section of the SDP we are parsing
    enum class Section { session_description, media_description };

    int version_ {};
    OriginField origin_;
    std::string session_name_;
    std::optional<ConnectionInfoField> connection_info_;
    TimeActiveField time_active_;
    std::optional<std::string> session_information_;
    std::optional<MediaDirection> media_direction_;
    std::optional<ReferenceClock> reference_clock_;
    std::optional<MediaClockSource> media_clock_;
    std::optional<RavennaClockDomain> clock_domain_;  // RAVENNA-specific attribute
    std::optional<uint32_t> sync_time_;               // RAVENNA-specific attribute
    std::vector<SourceFilter> source_filters_;
    std::optional<Group> group_;
    std::map<std::string, std::string> attributes_;  // Remaining, unknown attributes
    std::vector<MediaDescription> media_descriptions_;

    static tl::expected<int, std::string> parse_version(std::string_view line);
    tl::expected<void, std::string> parse_attribute(std::string_view line);
};

}  // namespace rav::sdp
