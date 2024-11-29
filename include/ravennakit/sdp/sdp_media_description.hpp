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

#include <cstdint>
#include <string>

#include "detail/sdp_media_clock_source.hpp"
#include "ravennakit/core/result.hpp"
#include "ravennakit/core/string_parser.hpp"
#include "detail/sdp_reference_clock.hpp"
#include "detail/sdp_source_filter.hpp"
#include "ravennakit/core/audio/audio_format.hpp"

#include <map>

namespace rav::sdp {

/**
 * Holds the information of an RTP map.
 */
struct format {
    int8_t payload_type {-1};
    std::string encoding_name;
    uint32_t clock_rate {};
    uint32_t num_channels {};

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    [[nodiscard]] std::string to_string() const;

    /**
     * @return The format as audio_format, or nullopt if the format is not supported or cannot be converted.
     */
    [[nodiscard]] std::optional<audio_format> to_audio_format() const;

    /**
     * Parses a format from a string.
     * @param line The string to parse.
     * @return A result indicating success or failure. When parsing fails, the error message will contain a
     * description of what went wrong.
     */
    static parse_result<format> parse_new(std::string_view line);

    friend bool operator==(const format& lhs, const format& rhs);
    friend bool operator!=(const format& lhs, const format& rhs);
};

inline auto format_as(const format& f) {
    return f.to_string();
}

/**
 * A type representing the connection information (c=*) of an SDP session description.
 */
struct connection_info_field {
    /// Specifies the type of network.
    netw_type network_type {netw_type::undefined};
    /// Specifies the type of address.
    addr_type address_type {addr_type::undefined};
    /// The address at which the media can be found.
    std::string address;
    /// Optional ttl
    std::optional<int32_t> ttl;
    /// Optional number of addresses
    std::optional<int32_t> number_of_addresses;

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    /**
     * Parses a connection info field from a string.
     * @param line The string to parse.
     * @return A pair containing the parse result and the connection info. When parsing fails, the connection info
     * will be a default-constructed object.
     */
    static parse_result<connection_info_field> parse_new(std::string_view line);
};

/**
 * A type representing the time field (t=*) of an SDP session description.
 * Defined as seconds since January 1, 1900, UTC.
 */
struct time_active_field {
    /// The start time of the session.
    int64_t start_time {-1};
    /// The stop time of the session.
    int64_t stop_time {-1};

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    /**
     * Parses a time field from a string.
     * @param line The string to parse.
     * @return A pair containing the parse result and the time field.
     */
    static parse_result<time_active_field> parse_new(std::string_view line);
};

/**
 * Defines a clock source and domain. This is a RAVENNA-specific attribute extension to the SDP specification.
 */
struct ravenna_clock_domain {
    static constexpr auto k_attribute_name = "clock-domain";
    enum class sync_source { undefined, ptp_v2 };

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    sync_source source {sync_source::undefined};
    int32_t domain {};

    static parse_result<ravenna_clock_domain> parse_new(std::string_view line);
};

/**
 * A type representing a media description (m=*) as part of an SDP session description.
 */
class media_description {
  public:
    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    /**
     * Parses a media description from a string (i.e. the line starting with m=*). Does not parse the connection
     * into or attributes.
     * @param line The string to parse.
     * @returns A result indicating success or failure. When parsing fails, the error message will contain a
     * description of the error.
     */
    static parse_result<media_description> parse_new(std::string_view line);

    /**
     * Parse an attribute from a string.
     * @param line The string to parse.
     * @return A result indicating success or failure. When parsing fails, the error message will contain a
     * description of the error.
     */
    parse_result<void> parse_attribute(std::string_view line);

    /**
     * @returns The media type of the media description (i.e. audio, video, text, application, message).
     */
    [[nodiscard]] const std::string& media_type() const;

    /**
     * @return The port number of the media description.
     */
    [[nodiscard]] uint16_t port() const;

    /**
     * @return The number of ports
     */
    [[nodiscard]] uint16_t number_of_ports() const;

    /**
     * @return The protocol of the media description.
     */
    [[nodiscard]] const std::string& protocol() const;

    /**
     * @return The formats of the media description.
     */
    [[nodiscard]] const std::vector<format>& formats() const;

    /**
     * Multiple addresses or "c=" lines MAY be specified on a per media description basis only if they provide multicast
     * addresses for different layers in a hierarchical or layered encoding scheme.
     * https://datatracker.ietf.org/doc/html/rfc8866#name-connection-information-c
     * @return The connection information of the media description.
     */
    [[nodiscard]] const std::vector<connection_info_field>& connection_infos() const;

    /**
     * Adds a connection info to the media description.
     * @param connection_info The connection info to add.
     */
    void add_connection_info(connection_info_field connection_info);

    /**
     * Sets the session information of the media description.
     * @param session_information The session information to set.
     */
    void set_session_information(std::string session_information);

    /**
     * @returns The value of the "ptime" attribute, or an empty optional if the attribute does not exist or the
     * value is invalid.
     */
    [[nodiscard]] std::optional<double> ptime() const;

    /**
     * @return The value of the "maxptime" attribute, or an empty optional if the attribute does not exist or the
     * value is invalid.
     */
    [[nodiscard]] std::optional<double> max_ptime() const;

    /**
     * @return The direction of the media description.
     */
    [[nodiscard]] const std::optional<media_direction>& direction() const;

    /**
     * @return The reference clock of the media description.
     */
    [[nodiscard]] const std::optional<reference_clock>& ref_clock() const;

    /**
     * @return The media clock of the media description.
     */
    [[nodiscard]] const std::optional<media_clock_source>& media_clock() const;

    /**
     * @return The session information of the media description.
     */
    [[nodiscard]] const std::optional<std::string>& session_information() const;

    /**
     * @return The sync-time of the stream. This is a RAVENNA-specific attribute extension.
     */
    [[nodiscard]] std::optional<uint32_t> sync_time() const;

    /**
     * @return The clock deviation of the stream. This is a RAVENNA-specific attribute extension.
     */
    [[nodiscard]] const std::optional<fraction<uint32_t>>& clock_deviation() const;

    /**
     * @returns The source filters of the media description.
     */
    [[nodiscard]] const std::vector<source_filter>& source_filters() const;

    /**
     * Returns framecount attribute. This is a legacy RAVENNA attribute, replaced by ptime. Only use when ptime is not
     * available.
     * @return The frame count of the media description.
     */
    [[nodiscard]] std::optional<uint32_t> framecount() const;

    /**
     * @returns Attributes which have not been parsed into a specific field.
     */
    [[nodiscard]] const std::map<std::string, std::string>& attributes() const;

  private:
    std::string media_type_;
    uint16_t port_ {};
    uint16_t number_of_ports_ {};
    std::string protocol_;
    std::vector<format> formats_;
    std::vector<connection_info_field> connection_infos_;
    std::optional<double> ptime_;
    std::optional<double> max_ptime_;
    std::optional<media_direction> media_direction_;
    std::optional<reference_clock> reference_clock_;
    std::optional<media_clock_source> media_clock_;
    std::optional<std::string> session_information_;
    std::optional<ravenna_clock_domain> clock_domain_;   // RAVENNA-specific attribute
    std::optional<uint32_t> sync_time_;                  // RAVENNA-specific attribute
    std::optional<fraction<uint32_t>> clock_deviation_;  // RAVENNA-specific attribute
    std::vector<source_filter> source_filters_;
    std::optional<uint32_t> framecount_;                  // Legacy RAVENNA attribute, replaced by ptime
    std::map<std::string, std::string> attributes_;  // Remaining, unknown attributes
};

}  // namespace rav::sdp
