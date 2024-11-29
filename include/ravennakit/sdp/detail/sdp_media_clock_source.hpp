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

#include "ravennakit/core/fraction.hpp"
#include "ravennakit/core/result.hpp"

namespace rav::sdp {

/**
 * The media clock source for a stream determines the timebase used to advance the RTP timestamps included in RTP
 * packets.
 * https://datatracker.ietf.org/doc/html/rfc7273#autoid-15
 */
class media_clock_source {
  public:
    static constexpr auto k_attribute_name = "mediaclk";

    enum class clock_mode { undefined, direct };

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    static parse_result<media_clock_source> parse_new(std::string_view line);

    /**
     * @returns The clock mode
     */
    [[nodiscard]] clock_mode mode() const;

    /**
     * @return The offset of the media clock.
     */
    [[nodiscard]] std::optional<int64_t> offset() const;

    /**
     * @return The rate numerator of the media clock.
     */
    [[nodiscard]] const std::optional<fraction<int32_t>>& rate() const;

  private:
    clock_mode mode_ {clock_mode::undefined};
    std::optional<int64_t> offset_;
    std::optional<fraction<int32_t>> rate_;
};

}  // namespace rav::sdp
