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

#include "../core/range.hpp"

#include <cstdint>
#include <optional>

namespace rav {

struct ptp_profile {
    const char* profile_name {""};
    uint8_t profile_number {};
    uint8_t primary_version {};
    uint8_t revision_number {};
    uint8_t profile_identifier[6] {};
    const char* organisation_name {""};
    const char* source_identification {""};

    struct default_ds_struct {
        uint8_t domain_number_default {};
        uint8_t priority1_default {};
        uint8_t priority2_default {};
        bool slave_only_default {};
        uint16_t sdo_id_default {};
    } default_ds;

    struct port_ds_struct {
        int8_t log_announce_interval_default {};
        range<int8_t> log_announce_interval_range {};

        int8_t log_sync_interval_default {};
        range<int8_t> log_sync_interval_range {};

        int8_t log_min_delay_req_interval_default {};
        range<int8_t> log_min_delay_req_interval_range {};

        uint8_t announce_receipt_timeout_default {};
        range<uint8_t> announce_receipt_timeout_range {};

        std::optional<int8_t> log_pdelay_req_interval_default {};
        std::optional<range<int8_t>> log_pdelay_req_interval_range {};
    } port_ds;

    struct transparent_clock_default_ds_struct {
        uint8_t primary_domain = 0;
    };

    std::optional<transparent_clock_default_ds_struct> transparent_clock_default_ds;

    double t {};  // Variance algorithm (7.6.3.2)
};

static constexpr ptp_profile ptp_default_profile_1 {
    "Default delay request-response profile",
    1,
    1,
    0,
    {0x00, 0x1B, 0x19, 0x01, 0x01, 0x00},
    "This profile is specified by the IEEE Precise Networked Clock Synchronization Working Group of the IM/ST Committee.",
    "A copy can be obtained by ordering IEEE Std 1588-2019 from the IEEE Standards Organization https://standards.ieee.org.",
    {
        0,
        128,
        128,
        false,
        0,
    },
    {
        1,
        {0, 4},
        0,
        {-1, 1},
        0,
        {0, 5},
        3,
        {2, 10},
    },
    {},
    1.0,
};

static constexpr ptp_profile ptp_default_profile_2 {
    "Default delay peer-to-peer delay profile",
    2,
    1,
    0,
    {0x00, 0x1B, 0x19, 0x02, 0x01, 0x00},
    "This profile is specified by the IEEE Precise Networked Clock Synchronization Working Group of the IM/ST Committee.",
    "A copy can be obtained by ordering IEEE Std 1588-2019 from the IEEE Standards Organization https://standards.ieee.org.",
    {
        0,
        128,
        128,
        false,
        0,
    },
    {
        1,
        {0, 4},
        0,
        {-1, 1},
        0,
        {0, 5},
        3,
        {2, 10},
    },
    {{
        0,
    }},
    1.0,
};

static constexpr ptp_profile ptp_default_profile_3 {
    "High Accuracy Delay Request-Response Default PTP Profile",
    3,
    1,
    0,
    {0x00, 0x1B, 0x19, 0x03, 0x01, 0x00},
    "This profile is specified by the IEEE Precise Networked Clock Synchronization Working Group of the IM/ST Committee.",
    "A copy can be obtained by ordering IEEE Std 1588-2019 from the IEEE Standards Organization https://standards.ieee.org.",
    {
        0,
        128,
        128,
        false,
        0,
    },
    {
        1,
        {0, 4},
        0,
        {-1, 1},
        0,
        {0, 5},
        3,
        {2, 10},
        {0},
        {{0, 5}},
    },
    {{
        0,
    }},
    1.0,
};

}  // namespace rav
