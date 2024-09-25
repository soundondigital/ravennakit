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
        static parse_result<origin_field> parse(const std::string& line);
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
        static parse_result<connection_info_field> parse(const std::string& line);
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
         * @return A pair containing the parse result and the time field. When parsing fails, the time field is a
         */
        static parse_result<time_active_field> parse(const std::string& line);
    };

    /**
     * Parses an SDP session description from a string.
     * @param sdp_text The SDP text to parse.
     * @return A pair containing the parse result and the session description. When parsing fails, the session
     * description is a default-constructed object.
     */
    static parse_result<session_description> parse(const std::string& sdp_text);

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

  private:
    int version_ {};
    origin_field origin_;
    std::string session_name_;
    std::optional<connection_info_field> connection_info_;
    time_active_field time_active_;

    static parse_result<int> parse_version(std::string_view line);
};

}  // namespace rav
