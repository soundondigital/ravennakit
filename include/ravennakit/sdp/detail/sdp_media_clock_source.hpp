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

#include <string_view>

#include "ravennakit/core/math/fraction.hpp"

#include "ravennakit/core/expected.hpp"

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

    MediaClockSource() = default;
    MediaClockSource(ClockMode mode, std::optional<int64_t> offset, std::optional<Fraction<int32_t>> rate);

    static tl::expected<MediaClockSource, std::string> parse_new(std::string_view line);

    /**
     * @returns The clock mode
     */
    [[nodiscard]] ClockMode mode() const;

    /**
     * @return The offset of the media clock.
     */
    [[nodiscard]] std::optional<int64_t> offset() const;

    /**
     * @return The rate numerator of the media clock.
     */
    [[nodiscard]] const std::optional<Fraction<int32_t>>& rate() const;

    /**
     * Validates the media clock source.
     * @throws rav::Exception if the media clock source is invalid.
     */
    [[nodiscard]] tl::expected<void, std::string> validate() const;

    /**
     * Converts the media clock source to a string.
     * @return The media clock source as a string.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string() const;

    /**
     * Converts the clock mode to a string.
     * @param mode The clock mode to convert.
     * @return The clock mode as a string.
     */
    static std::string to_string(ClockMode mode);

  private:
    ClockMode mode_ {ClockMode::undefined};
    std::optional<int64_t> offset_;
    std::optional<Fraction<int32_t>> rate_;
};

}  // namespace rav::sdp
