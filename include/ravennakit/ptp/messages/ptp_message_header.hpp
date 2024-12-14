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
#include "ravennakit/ptp/ptp_definitions.hpp"
#include "ravennakit/ptp/ptp_error.hpp"
#include "ravennakit/ptp/ptp_types.hpp"

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
    uint16_t sequence_id {};
    int8_t logMessageInterval {};

    static tl::expected<ptp_message_header, ptp_error> from_data(buffer_view<const uint8_t> data);

    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] auto tie_members() const;
};

bool operator==(const ptp_message_header::flag_field& lhs, const ptp_message_header::flag_field& rhs);
bool operator!=(const ptp_message_header::flag_field& lhs, const ptp_message_header::flag_field& rhs);

bool operator==(const ptp_message_header& lhs, const ptp_message_header& rhs);
bool operator!=(const ptp_message_header& lhs, const ptp_message_header& rhs);

}  // namespace rav
