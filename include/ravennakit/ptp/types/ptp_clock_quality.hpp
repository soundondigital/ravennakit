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

#include "ravennakit/ptp/ptp_definitions.hpp"

namespace rav {

/**
 * PTP Clock Quality
 * IEEE1588-2019 section 7.6.2.5, Table 4
 */
struct ptp_clock_quality {
    /// The clock class. Default is 248, for slave-only the value is 255.
    uint8_t clock_class {};
    ptp_clock_accuracy clock_accuracy {ptp_clock_accuracy::unknown};
    uint16_t offset_scaled_log_variance {};

    /**
     * Create a PTP clock quality from a buffer_view.
     * @param stream The stream to read the clock quality from.
     */
    void write_to(output_stream& stream) const {
        stream.write_be<uint8_t>(clock_class);
        stream.write_be<uint8_t>(static_cast<uint8_t>(clock_accuracy));
        stream.write_be<uint16_t>(offset_scaled_log_variance);
    }

    /**
     * Create a PTP clock quality from a buffer_view.
     * @return A PTP clock quality if the data is valid, otherwise a PTP error.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format(
            "clock_class={} clock_accuracy={} offset_scaled_log_variance={}", clock_class,
            rav::to_string(clock_accuracy), offset_scaled_log_variance
        );
    }
};

}  // namespace rav
