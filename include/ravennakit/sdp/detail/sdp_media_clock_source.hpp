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

#include "ravennakit/core/math/fraction.hpp"
#include "ravennakit/core/expected.hpp"

#include <string_view>

namespace rav::sdp {

/**
 * The media clock source for a stream determines the timebase used to advance the RTP timestamps included in RTP
 * packets.
 * https://datatracker.ietf.org/doc/html/rfc7273#autoid-15
 */
class MediaClockSource {
  public:
    static constexpr auto k_attribute_name = "mediaclk";

    enum class ClockMode { undefined, direct };

    ClockMode mode {ClockMode::undefined};
    std::optional<int64_t> offset;
    std::optional<Fraction<int32_t>> rate;
};

/**
 * Converts the media clock source to a string.
 * @return The media clock source as a string.
 */
[[nodiscard]] std::string to_string(const MediaClockSource& mode);

/**
 * Converts the clock mode to a string.
 * @param mode The clock mode to convert.
 * @return The clock mode as a string.
 */
[[nodiscard]] const char* to_string(MediaClockSource::ClockMode mode);

/**
 * Validates the media clock source.
 * @throws rav::Exception if the media clock source is invalid.
 */
[[nodiscard]] tl::expected<void, std::string> validate(const MediaClockSource& clock_source);

/**
 * Create a MediaClockSource from a string from an SDP.
 * @param line The input text.
 * @return The MediaClockSource, or an error.
 */
[[nodiscard]] tl::expected<MediaClockSource, std::string> parse_media_clock_source(std::string_view line);

}  // namespace rav::sdp
