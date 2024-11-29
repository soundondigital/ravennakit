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
     * Parses a reference clock from a string.
     * @param line The string to parse.
     * @return A parse result.
     */
    static parse_result<reference_clock> parse_new(std::string_view line);

private:
    clock_source source_ {clock_source::undefined};
    std::optional<ptp_ver> ptp_version_ {ptp_ver::undefined};
    std::optional<std::string> gmid_;
    std::optional<int32_t> domain_ {};
};

}  // namespace rav::sdp
