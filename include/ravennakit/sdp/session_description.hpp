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

#include "ravennakit/core/result.hpp"
#include "ravennakit/core/string.hpp"

namespace rav {

/**
 * A class that represents an SDP session description.
 */
class session_description {
  public:
    enum class netw_type { undefined, internet };
    enum class addr_type { undefined, ipv4, ipv6 };
    enum class media_direction { sendrecv, sendonly, recvonly, inactive };

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, const char*>;

    /**
     * A type which represents the origin field (o=*) of an SDP session description.
     * In general, the origin serves as a globally unique identifier for this version of the session description, and
     * the subfields excepting the version, taken together identify the session irrespective of any modifications.
     */
    struct origin_field {
        /// The user's login on the originating host, or "-" if the originating host does not support the concept of
        /// user IDs.
        std::string username;

        /// Holds a numeric string such that the tuple of <username>, <sess-id>, <nettype>, <addrtype>, and
        /// <unicast-address> forms a globally unique identifier for the session.
        std::string session_id;

        /// The version number for this session description.
        int session_version {};

        /// Specifies the type of network.
        netw_type network_type {netw_type::undefined};

        /// Specifies the type of address.
        addr_type address_type {addr_type::undefined};

        /// The address of the machine from which the session was created.
        std::string unicast_address;

        /**
         * Parses an origin field from a string.
         * @param line The string to parse.
         * @return A result indicating success or failure. When parsing fails, the error message will contain a
         * description of the error.
         */
        static parse_result<origin_field> parse_new(const std::string& line);
    };

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
        std::optional<int> ttl;
        /// Optional number of addresses
        std::optional<int> number_of_addresses;

        /**
         * Parses a connection info field from a string.
         * @param line The string to parse.
         * @return A pair containing the parse result and the connection info. When parsing fails, the connection info
         * will be a default-constructed object.
         */
        static parse_result<connection_info_field> parse_new(const std::string& line);
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

        /**
         * Parses a time field from a string.
         * @param line The string to parse.
         * @return A pair containing the parse result and the time field.
         */
        static parse_result<time_active_field> parse_new(const std::string& line);
    };

    /**
     * Holds the information of an RTP map.
     */
    struct format {
        int8_t payload_type {-1};
        std::string encoding_name;
        int32_t clock_rate {};
        int32_t channels {};

        static parse_result<format> parse_new(const std::string& line);
    };

    /**
     * A type representing a media description (m=*) of an SDP session description.
     */
    struct media_description {
        /**
         * Parses a media description from a string (i.e. the line starting with m=*). Does not parse the connection
         * into or attributes.
         * @param line The string to parse.
         * @returns A result indicating success or failure. When parsing fails, the error message will contain a
         * description of the error.
         */
        static parse_result<media_description> parse_new(const std::string& line);

        /**
         * Parse an attribute from a string.
         * @param line The string to parse.
         * @return A result indicating success or failure. When parsing fails, the error message will contain a
         * description of the error.
         */
        parse_result<void> parse_attribute(const std::string& line);

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
         * @return The connection information of the media description.
         */
        [[nodiscard]] const std::vector<connection_info_field>& connection_infos() const;

        /**
         * Adds a connection info to the media description.
         * @param connection_info The connection info to add.
         */
        void add_connection_info(connection_info_field connection_info);

        /**
         * @returns The value of the "ptime" attribute, or an empty optional if the attribute does not exist or the
         * value is invalid.
         */
        [[nodiscard]] std::optional<double> ptime() const;

        /**
         * @return The direction of the media description.
         */
        [[nodiscard]] std::optional<media_direction> direction() const;

      private:
        std::string media_type_;
        uint16_t port_ {};
        uint16_t number_of_ports_ {};
        std::string protocol_;
        std::vector<format> formats_;
        std::vector<connection_info_field> connection_infos_;
        std::optional<double> ptime_;
        std::optional<media_direction> media_direction_;
    };

    /**
     * Parses an SDP session description from a string.
     * @param sdp_text The SDP text to parse.
     * @return A pair containing the parse result and the session description. When parsing fails, the session
     * description is a default-constructed object.
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

  private:
    /// Type to specify which section of the SDP we are parsing
    enum class section { session_description, media_description };

    int version_ {};
    origin_field origin_;
    std::string session_name_;
    std::optional<connection_info_field> connection_info_;
    time_active_field time_active_;
    std::vector<media_description> media_descriptions_;
    std::optional<media_direction> media_direction_;

    static parse_result<int> parse_version(std::string_view line);
    parse_result<void> parse_attribute(const std::string& line);
};

}  // namespace rav
