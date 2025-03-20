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
class ReferenceClock {
  public:
    enum class ClockSource { undefined, atomic_clock, gps, terrestrial_radio, ptp, ntp, ntp_server, ntp_pool };
    enum class PtpVersion { undefined, IEEE_1588_2002, IEEE_1588_2008, IEEE_802_1AS_2011, traceable };

    /// A type alias for a parse result.
    template<class T>
    using ParseResult = result<T, std::string>;

    ReferenceClock() = default;
    ReferenceClock(ClockSource source, PtpVersion version, std::string gmid, int32_t domain);

    /**
     * @returns The source of the reference clock.
     */
    [[nodiscard]] ClockSource source() const;

    /**
     * @return The PTP version of the reference clock.
     */
    [[nodiscard]] std::optional<PtpVersion> ptp_version() const;

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
    static ParseResult<ReferenceClock> parse_new(std::string_view line);

    /**
     * @param source The clock source to convert.
     * @returns A string representation of the clock source.
     */
    static std::string to_string(ClockSource source);

    /**
     * @param version The PTP version to convert.
     * @return A string representation of the PTP version.
     */
    static std::string to_string(PtpVersion version);

  private:
    ClockSource source_ {ClockSource::undefined};
    std::optional<PtpVersion> ptp_version_ {PtpVersion::undefined};
    std::optional<std::string> gmid_;
    std::optional<int32_t> domain_ {};
};

}  // namespace rav::sdp
