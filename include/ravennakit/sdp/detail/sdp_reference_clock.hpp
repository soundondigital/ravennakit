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

#include "ravennakit/core/expected.hpp"

namespace rav::sdp {

/**
 * Specifies the reference clock as specified in RFC 7273 and AES67.
 * https://datatracker.ietf.org/doc/html/rfc7273#section-4.3
 */
class ReferenceClock {
  public:
    enum class ClockSource { undefined, atomic_clock, gps, terrestrial_radio, ptp, ntp, ntp_server, ntp_pool };
    enum class PtpVersion { undefined, IEEE_1588_2002, IEEE_1588_2008, IEEE_802_1AS_2011, traceable };

    ClockSource source_ {ClockSource::undefined};
    std::optional<PtpVersion> ptp_version_ {PtpVersion::undefined};
    std::optional<std::string> gmid_;
    std::optional<int32_t> domain_ {};
};

/**
 * @param source The clock source to convert.
 * @returns A string representation of the clock source.
 */
[[nodiscard]] const char* to_string(ReferenceClock::ClockSource source);

/**
 * @param version The PTP version to convert.
 * @return A string representation of the PTP version.
 */
[[nodiscard]] const char* to_string(ReferenceClock::PtpVersion version);

/**
 * Converts the reference clock to a string.
 * @return The reference clock as a string.
 */
[[nodiscard]] std::string to_string(const ReferenceClock& reference_clock);

/**
 * Parses a reference clock from a string.
 * @param line The string to parse.
 * @return A parse result.
 */
[[nodiscard]] tl::expected<ReferenceClock, std::string> parse_reference_clock(std::string_view line);

/**
 * Validates the state of this object.
 * @return An error message if the state is invalid.
 */
[[nodiscard]] tl::expected<void, std::string> validate(const ReferenceClock& reference_clock);

}  // namespace rav::sdp
