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

#include <sstream>
#include <vector>

#include "sdp_media_description.hpp"
#include "detail/sdp_constants.hpp"
#include "ravennakit/core/result.hpp"
#include "detail/sdp_reference_clock.hpp"
#include "detail/sdp_origin.hpp"
#include "detail/sdp_time_active.hpp"

namespace rav::sdp {

/**
 * A class that represents an SDP session description as defined in RFC 8866.
 * https://datatracker.ietf.org/doc/html/rfc8866
 */
class session_description {
  public:
    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    /**
     * Parses an SDP session description from a string.
     * @param sdp_text The SDP text to parse.
     * @return A result indicating whether the parsing was successful or not. The error will be a message explaining
     * what went wrong.
     */
    static parse_result<session_description> parse_new(const std::string& sdp_text);

    /**
     * @returns The version of the SDP session description.
     */
    [[nodiscard]] int version() const;

    /**
     * @returns The origin of the SDP session description.
     */
    [[nodiscard]] const origin_field& origin() const;

    /**
     * Sets the origin of the SDP session description.
     * @param origin The origin to set.
     */
    void set_origin(origin_field origin);

    /**
     * @return The connection information of the SDP session description.
     */
    [[nodiscard]] const std::optional<connection_info_field>& connection_info() const;

    /**
     * Sets the connection information of the SDP session description.
     * @param connection_info The connection information to set.
     */
    void set_connection_info(connection_info_field connection_info);

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
    [[nodiscard]] time_active_field time_active() const;

    /**
     * Sets the time field of the SDP session description.
     * @param time_active The time field to set.
     */
    void set_time_active(time_active_field time_active);

    /**
     * @returns The media descriptions of the SDP session description.
     */
    [[nodiscard]] const std::vector<media_description>& media_descriptions() const;

    /**
     * @return The direction of the media description. If the direction is not specified, the return value is sendrecv
     * which is the default as specified in RFC 8866 section 6.7).
     */
    [[nodiscard]] media_direction direction() const;

    /**
     * @return The reference clock of the session description.
     */
    [[nodiscard]] std::optional<reference_clock> ref_clock() const;

    /**
     * Sets the reference clock of the session description.
     * @param ref_clock The reference clock to set.
     */
    void set_ref_clock(reference_clock ref_clock);

    /**
     * @return The media clock of the session description.
     */
    [[nodiscard]] const std::optional<media_clock_source>& media_clock() const;

    /**
     * Sets the media clock of the session description.
     * @param media_clock The media clock to set.
     */
    void set_media_clock(media_clock_source media_clock);

    /**
     * @return The clock domain of the session description. This is a RAVENNA-specific attribute extension.
     */
    [[nodiscard]] const std::optional<ravenna_clock_domain>& clock_domain() const;

    /**
     * Sets the clock domain of the session description.
     * @param clock_domain The clock domain to set.
     */
    void set_clock_domain(ravenna_clock_domain clock_domain);

    /**
     * @returns The source filters of the media description.
     */
    [[nodiscard]] const std::vector<source_filter>& source_filters() const;

    /**
     * Adds a source filter to the session description. If the filter already exists, it will be replaced.
     * @param filter The source filter to add.
     */
    void add_source_filter(const source_filter& filter);

    /**
     * @return The media direction of the session description.
     */
    [[nodiscard]] std::optional<media_direction> get_media_direction() const;

    /**
     * Sets the media direction of the session description.
     * @param direction The media direction to set.
     */
    void set_media_direction(media_direction direction);

    /**
     * @returns Attributes which have not been parsed into a specific field.
     */
    [[nodiscard]] const std::map<std::string, std::string>& attributes() const;
    
    /**
     * Converts the session description to a string.
     * @return The session description as a string.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string(const char* newline = k_sdp_crlf) const;

  private:
    /// Type to specify which section of the SDP we are parsing
    enum class section { session_description, media_description };

    int version_ {};
    origin_field origin_;
    std::string session_name_;
    std::optional<connection_info_field> connection_info_;
    time_active_field time_active_;
    std::optional<std::string> session_information_;
    std::optional<media_direction> media_direction_;
    std::optional<reference_clock> reference_clock_;
    std::optional<media_clock_source> media_clock_;
    std::optional<ravenna_clock_domain> clock_domain_;
    std::vector<source_filter> source_filters_;
    std::map<std::string, std::string> attributes_;  // Remaining, unknown attributes
    std::vector<media_description> media_descriptions_;

    static parse_result<int> parse_version(std::string_view line);
    parse_result<void> parse_attribute(std::string_view line);
};

}  // namespace rav::sdp
