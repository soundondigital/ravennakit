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

#include "media_description.hpp"
#include "ravennakit/core/result.hpp"
#include "reference_clock.hpp"

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
     * @return The connection information of the SDP session description.
     */
    [[nodiscard]] std::optional<connection_info_field> connection_info() const;

    /**
     * @returns The session name of the SDP session description.
     */
    [[nodiscard]] std::string session_name() const;

    /**
     * @return The time field of the SDP session description.
     */
    [[nodiscard]] time_active_field time_active() const;

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
     * @return The media clock of the session description.
     */
    [[nodiscard]] const std::optional<media_clock_source>& media_clock() const;

    /**
     * @return The clock domain of the session description. This is a RAVENNA-specific attribute extension.
     */
    [[nodiscard]] const std::optional<ravenna_clock_domain>& clock_domain() const;

  private:
    /// Type to specify which section of the SDP we are parsing
    enum class section { session_description, media_description };

    int version_ {};
    origin_field origin_;
    std::string session_name_;
    std::optional<connection_info_field> connection_info_;
    time_active_field time_active_;
    std::optional<std::string> session_information_;
    std::vector<media_description> media_descriptions_;
    std::optional<media_direction> media_direction_;
    std::optional<reference_clock> reference_clock_;
    std::optional<sdp::media_clock_source> media_clock_;
    std::optional<ravenna_clock_domain> clock_domain_;

    static parse_result<int> parse_version(std::string_view line);
    parse_result<void> parse_attribute(std::string_view line);
};

}  // namespace rav::sdp
