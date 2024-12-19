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

#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/core/streams/output_stream.hpp"
#include "ravennakit/core/util/sequence_number.hpp"
#include "ravennakit/ptp/ptp_definitions.hpp"
#include "ravennakit/ptp/ptp_error.hpp"
#include "ravennakit/ptp/types/ptp_port_identity.hpp"

#include <cstdint>
#include <tl/expected.hpp>

namespace rav {

struct ptp_version {
    uint8_t major {};
    uint8_t minor {};
};

bool operator==(const ptp_version& lhs, const ptp_version& rhs);
bool operator!=(const ptp_version& lhs, const ptp_version& rhs);

/**
 * Provides a view over given data, interpreting it as a PTP message header.
 */
struct ptp_message_header {
    constexpr static size_t k_header_size = 34;

    struct flag_field {
        bool alternate_master_flag {};      // Announce, Sync, Follow_Up, Delay_Resp
        bool two_step_flag {};              // Sync, Pdelay_resp
        bool unicast_flag {};               // All
        bool profile_specific_1 {};         // All
        bool profile_specific_2 {};         // All
        bool leap61 {};                     // Announce
        bool leap59 {};                     // Announce
        bool current_utc_offset_valid {};   // Announce
        bool ptp_timescale {};              // Announce
        bool time_traceable {};             // Announce
        bool frequency_traceable {};        // Announce
        bool synchronization_uncertain {};  // Announce

        static flag_field from_octets(uint8_t octet1, uint8_t octet2);
        [[nodiscard]] uint16_t to_octets() const;

        [[nodiscard]] auto tie_members() const;
    };

    uint16_t sdo_id {};
    ptp_message_type message_type {};
    ptp_version version;
    uint16_t message_length {};
    uint8_t domain_number {};
    flag_field flags;
    int64_t correction_field {};
    ptp_port_identity source_port_identity;
    uint16_t sequence_id{};
    int8_t log_message_interval {};

    /**
     * Creates a PTP message header from the given data.
     * @param data The data to interpret as a PTP message header.
     * @return A PTP message header if the data is valid, otherwise an error.
     */
    static tl::expected<ptp_message_header, ptp_error> from_data(buffer_view<const uint8_t> data);

    /**
     * Writes the PTP message header to the given stream.
     * @param stream The stream to write the PTP message header to.
     */
    void write_to(output_stream& stream) const;

    /**
     * Converts the PTP message header to a human-readable string.
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * Returns a tuple of the members of the PTP message header.
     */
    [[nodiscard]] auto tie_members() const;
};

bool operator==(const ptp_message_header::flag_field& lhs, const ptp_message_header::flag_field& rhs);
bool operator!=(const ptp_message_header::flag_field& lhs, const ptp_message_header::flag_field& rhs);

bool operator==(const ptp_message_header& lhs, const ptp_message_header& rhs);
bool operator!=(const ptp_message_header& lhs, const ptp_message_header& rhs);

}  // namespace rav
