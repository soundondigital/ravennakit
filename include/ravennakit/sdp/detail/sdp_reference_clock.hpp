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

#include <string>

#include "ravennakit/core/result.hpp"

#include <tl/expected.hpp>

namespace rav::sdp {

/**
 * Specifies the reference clock as specified in RFC 7273 and AES67.
 * https://datatracker.ietf.org/doc/html/rfc7273#section-4.3
 */
class reference_clock {
  public:
    enum class clock_source { undefined, atomic_clock, gps, terrestrial_radio, ptp, ntp, ntp_server, ntp_pool };
    enum class ptp_ver { undefined, IEEE_1588_2002, IEEE_1588_2008, IEEE_802_1AS_2011, traceable };

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    reference_clock() = default;
    reference_clock(clock_source source, ptp_ver version, std::string gmid, int32_t domain);

    /**
     * @returns The source of the reference clock.
     */
    [[nodiscard]] clock_source source() const;

    /**
     * @return The PTP version of the reference clock.
     */
    [[nodiscard]] std::optional<ptp_ver> ptp_version() const;

    /**
     * @return The GMID of the reference clock.
     */
    [[nodiscard]] const std::optional<std::string>& gmid() const;

    /**
     * @return The domain of the reference clock.
     */
    [[nodiscard]] const std::optional<int32_t>& domain() const;

    /**
     * Validates the state of this object.
     * @return An error message if the state is invalid.
     */
    [[nodiscard]] tl::expected<void, std::string> validate() const;

    /**
     * Converts the reference clock to a string.
     * @return The reference clock as a string.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string() const;

    /**
     * Parses a reference clock from a string.
     * @param line The string to parse.
     * @return A parse result.
     */
    static parse_result<reference_clock> parse_new(std::string_view line);

    /**
     * @param source The clock source to convert.
     * @returns A string representation of the clock source.
     */
    static std::string to_string(clock_source source);

    /**
     * @param version The PTP version to convert.
     * @return A string representation of the PTP version.
     */
    static std::string to_string(ptp_ver version);

  private:
    clock_source source_ {clock_source::undefined};
    std::optional<ptp_ver> ptp_version_ {ptp_ver::undefined};
    std::optional<std::string> gmid_;
    std::optional<int32_t> domain_ {};
};

}  // namespace rav::sdp
