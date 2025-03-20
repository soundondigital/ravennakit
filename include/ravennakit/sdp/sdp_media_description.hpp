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

#include "detail/sdp_connection_info.hpp"
#include "detail/sdp_constants.hpp"
#include "detail/sdp_format.hpp"

#include <cstdint>
#include <string>

#include "detail/sdp_media_clock_source.hpp"
#include "detail/sdp_ravenna_clock_domain.hpp"
#include "ravennakit/core/result.hpp"
#include "ravennakit/core/string_parser.hpp"
#include "detail/sdp_reference_clock.hpp"
#include "detail/sdp_source_filter.hpp"

#include <map>

namespace rav::sdp {

/**
 * A type representing a media description (m=*) as part of an SDP session description.
 */
class MediaDescription {
  public:
    /// A type alias for a parse result.
    /// TODO: Replace with tl::expected
    template<class T>
    using ParseResult = result<T, std::string>;

    /**
     * Parses a media description from a string (i.e. the line starting with m=*). Does not parse the connection
     * into or attributes.
     * @param line The string to parse.
     * @returns A result indicating success or failure. When parsing fails, the error message will contain a
     * description of the error.
     */
    static ParseResult<MediaDescription> parse_new(std::string_view line);

    /**
     * Parse an attribute from a string.
     * @param line The string to parse.
     * @return A result indicating success or failure. When parsing fails, the error message will contain a
     * description of the error.
     */
    ParseResult<void> parse_attribute(std::string_view line);

    /**
     * @returns The media type of the media description (i.e. audio, video, text, application, message).
     */
    [[nodiscard]] const std::string& media_type() const;

    /**
     * Sets the media type.
     * @param media_type The media type to set.
     */
    void set_media_type(std::string media_type);

    /**
     * @return The port number of the media description.
     */
    [[nodiscard]] uint16_t port() const;

    /**
     * Sets the port number of the media description.
     * @param port The port number to set.
     */
    void set_port(uint16_t port);

    /**
     * @return The number of ports
     */
    [[nodiscard]] uint16_t number_of_ports() const;

    /**
     * Set number of ports. Must be 1 or higher.
     * @param number_of_ports The number of ports to set.
     */
    void set_number_of_ports(uint16_t number_of_ports);

    /**
     * @return The protocol of the media description.
     */
    [[nodiscard]] const std::string& protocol() const;

    /**
     * Sets the protocol of the media description.
     * @param protocol The protocol to set.
     */
    void set_protocol(std::string protocol);

    /**
     * @return The formats of the media description.
     */
    [[nodiscard]] const std::vector<Format>& formats() const;

    /**
     * Adds a format to the media description. If a format with the same payload type already exists, it will be
     * replaced.
     * @param format_to_add The format to add.
     */
    void add_format(const Format& format_to_add);

    /**
     * Multiple addresses or "c=" lines MAY be specified on a per media description basis only if they provide multicast
     * addresses for different layers in a hierarchical or layered encoding scheme.
     * https://datatracker.ietf.org/doc/html/rfc8866#name-connection-information-c
     * @return The connection information of the media description.
     */
    [[nodiscard]] const std::vector<ConnectionInfoField>& connection_infos() const;

    /**
     * Adds a connection info to the media description.
     * @param connection_info The connection info to add.
     */
    void add_connection_info(ConnectionInfoField connection_info);

    /**
     * @returns The value of the "ptime" attribute, or an empty optional if the attribute does not exist or the
     * value is invalid.
     */
    [[nodiscard]] std::optional<float> ptime() const;

    /**
     * Sets the ptime attribute.
     * @param ptime The ptime to set.
     */
    void set_ptime(std::optional<float> ptime);

    /**
     * @return The value of the "maxptime" attribute, or an empty optional if the attribute does not exist or the
     * value is invalid.
     */
    [[nodiscard]] std::optional<float> max_ptime() const;

    /**
     * Sets the maxptime attribute.
     * @param max_ptime The maxptime to set.
     */
    void set_max_ptime(std::optional<float> max_ptime);

    /**
     * @return The direction of the media description.
     */
    [[nodiscard]] const std::optional<MediaDirection>& direction() const;

    /**
     * Sets the direction of the media description.
     * @param direction The direction to set.
     */
    void set_direction(MediaDirection direction);

    /**
     * @return The reference clock of the media description.
     */
    [[nodiscard]] const std::optional<ReferenceClock>& ref_clock() const;

    /**
     * Sets the reference clock of the media description.
     * @param ref_clock The reference clock to set.
     */
    void set_ref_clock(ReferenceClock ref_clock);

    /**
     * @return The media clock of the media description.
     */
    [[nodiscard]] const std::optional<MediaClockSource>& media_clock() const;

    /**
     * Sets the media clock of the media description.
     * @param media_clock The media clock to set.
     */
    void set_media_clock(MediaClockSource media_clock);

    /**
     * @return The session information of the media description.
     */
    [[nodiscard]] const std::optional<std::string>& session_information() const;

    /**
     * Sets the session information of the media description.
     * @param session_information The session information to set.
     */
    void set_session_information(std::string session_information);

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
     * @return The clock deviation of the stream. This is a RAVENNA-specific attribute extension.
     */
    [[nodiscard]] const std::optional<fraction<uint32_t>>& clock_deviation() const;

    /**
     * Sets the clock deviation of the stream. This is a RAVENNA-specific attribute extension.
     * @param clock_deviation The clock deviation to set.
     */
    void set_clock_deviation(std::optional<fraction<uint32_t>> clock_deviation);

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
     * Returns framecount attribute. This is a legacy RAVENNA attribute, replaced by ptime. Only use when ptime is not
     * available.
     * @return The frame count of the media description.
     */
    [[nodiscard]] std::optional<uint16_t> framecount() const;

    /**
     * Sets the frame count of the media description. This is a legacy RAVENNA attribute, replaced by ptime.
     * @param framecount The frame count to set.
     */
    void set_framecount(std::optional<uint32_t> framecount);

    /**
     * Returns the clock domain attribute. This is a RAVENNA-specific attribute and redundant to a=ts-refclk.
     * @return The clock domain of the media description.
     */
    [[nodiscard]]
    std::optional<RavennaClockDomain> clock_domain() const;

    /**
     * Sets the clock domain attribute. This is a RAVENNA-specific attribute and redundant to a=ts-refclk.
     * @param clock_domain The clock domain to set.
     */
    void set_clock_domain(RavennaClockDomain clock_domain);

    /**
     * @returns Attributes which have not been parsed into a specific field.
     */
    [[nodiscard]] const std::map<std::string, std::string>& attributes() const;

    /**
     * Validates the media description.
     * @return An error message if the media description is invalid.
     */
    [[nodiscard]] tl::expected<void, std::string> validate() const;

    /**
     * Converts the media description to a string.
     * @return The media description as a string.
     */
    tl::expected<std::string, std::string> to_string(const char* newline = k_sdp_crlf) const;

  private:
    std::string media_type_;
    uint16_t port_ {};
    uint16_t number_of_ports_ {1};
    std::string protocol_;
    std::vector<Format> formats_;
    std::vector<ConnectionInfoField> connection_infos_;
    std::optional<float> ptime_;
    std::optional<float> max_ptime_;
    std::optional<MediaDirection> media_direction_;
    std::optional<ReferenceClock> reference_clock_;
    std::optional<MediaClockSource> media_clock_;
    std::optional<std::string> session_information_;
    std::optional<RavennaClockDomain> clock_domain_;   // RAVENNA-specific attribute
    std::optional<uint32_t> sync_time_;                  // RAVENNA-specific attribute
    std::optional<fraction<uint32_t>> clock_deviation_;  // RAVENNA-specific attribute
    std::vector<SourceFilter> source_filters_;
    std::optional<uint32_t> framecount_;             // Legacy RAVENNA attribute, replaced by ptime
    std::map<std::string, std::string> attributes_;  // Remaining, unknown attributes
};

}  // namespace rav::sdp
