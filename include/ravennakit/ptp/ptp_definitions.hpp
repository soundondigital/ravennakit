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
#include <cstdint>

namespace rav {

/**
 * The PTP state.
 * IEEE1588-2019: 8.2.15.3.1, 9.2.5, Table 27
 */
enum class ptp_state : uint8_t {
    undefined = 0x0,
    initializing = 0x1,
    faulty = 0x2,
    disabled = 0x3,
    listening = 0x4,
    pre_master = 0x5,
    master = 0x6,
    passive = 0x7,
    uncalibrated = 0x8,
    slave = 0x9,
};

/**
 * IEEE1588-2019: 7.6.2.6, Table 5
 */
enum class ptp_clock_accuracy : uint8_t {
    // 0x00 to 0x16 Reserved
    lt_1_ps = 0x17,    // The time is accurate to within 1 picosecond
    lt_2_5_ps = 0x18,  // The time is accurate to within 2.5 picoseconds
    lt_10_ps = 0x19,   // The time is accurate to within 10 picoseconds
    lt_25_ps = 0x1a,   // The time is accurate to within 25 picoseconds
    lt_100_ps = 0x1b,  // The time is accurate to within 100 picoseconds
    lt_250_ps = 0x1c,  // The time is accurate to within 250 picoseconds
    lt_1_ns = 0x1d,    // The time is accurate to within 1 nanosecond
    lt_2_5_ns = 0x1e,  // The time is accurate to within 2.5 nanoseconds
    lt_10_ns = 0x1f,   // The time is accurate to within 10 nanoseconds
    lt_25_ns = 0x20,   // The time is accurate to within 25 nanoseconds
    lt_100_ns = 0x21,  // The time is accurate to within 100 nanoseconds
    lt_250_ns = 0x22,  // The time is accurate to within 250 nanoseconds
    lt_1_us = 0x23,    // The time is accurate to within 1 microsecond
    lt_2_5_us = 0x24,  // The time is accurate to within 2.5 microseconds
    lt_10_us = 0x25,   // The time is accurate to within 10 microseconds
    lt_25_us = 0x26,   // The time is accurate to within 25 microseconds
    lt_100_us = 0x27,  // The time is accurate to within 100 microseconds
    lt_250_us = 0x28,  // The time is accurate to within 250 microseconds
    lt_1_ms = 0x29,    // The time is accurate to within 1 millisecond
    lt_2_5_ms = 0x2a,  // The time is accurate to within 2.5 milliseconds
    lt_10_ms = 0x2b,   // The time is accurate to within 10 milliseconds
    lt_25_ms = 0x2c,   // The time is accurate to within 25 milliseconds
    lt_100_ms = 0x2d,  // The time is accurate to within 100 milliseconds
    lt_250_ms = 0x2e,  // The time is accurate to within 250 milliseconds
    lt_1_s = 0x2f,     // The time is accurate to within 1 second
    lt_10_s = 0x30,    // The time is accurate to within 10 seconds
    gt_10_s = 0x31,    // Greater than 10 seconds
    // 0x32 to 0x7f Reserved
    // 0x80 to 0xfd Designated for assignment by alternate PTP Profiles
    unknown = 0xFE,   // The accuracy of the time is unknown
    reserved = 0xFF,  // Reserved
};

/**
 * State decision codes.
 * IEEE1588-2019: sections 9.3.1, 9.3.5, Table 30, 31, 32, 33.
 */
enum class ptp_state_decision_code {
    /// The PTP Port is in the MASTER state because it is on a clockClass 1 through 127 PTP Instance and is a PTP Port
    /// of the Grandmaster PTP Instance of the domain.
    m1,
    /// The PTP Port is in the MASTER state because it is on a clockClass 128 or higher PTP Instance and is a PTP Port
    /// of the Grandmaster PTP Instance of the domain.
    m2,
    /// The PTP Port is in the MASTER state, but it is not a PTP Port on the Grandmaster PTP Instance of the domain.
    m3,
    /// The PTP Port is in the SLAVE state.
    s1,
    /// The PTP Port is in the PASSIVE state because it is on a clockClass 1 through 127 PTP Instance and is either not
    /// on the Grandmaster PTP Instance of the domain or is PASSIVE to break a timing loop.
    p1,
    /// The PTP Port is in the PASSIVE state because it is on a clockClass 128 or higher PTP Instance and is PASSIVE to
    /// break a timing loop.
    p2
};

/**
 * PTP Message types.
 * IEEE1588-2019: Table 36
 */
enum class ptp_message_type : uint8_t {
    sync = 0x0,          // Event
    delay_req = 0x1,     // Event
    p_delay_req = 0x2,   // Event
    p_delay_resp = 0x3,  // Event
    reserved1 = 0x4,
    reserved2 = 0x5,
    reserved3 = 0x6,
    reserved4 = 0x7,
    follow_up = 0x8,               // General
    delay_resp = 0x9,              // General
    p_delay_resp_follow_up = 0xa,  // General
    announce = 0xb,                // General
    signaling = 0xc,               // General
    management = 0xd,              // General
    reserved5 = 0xe,
    reserved6 = 0xf,
};

inline const char* ptp_message_type_to_string(const ptp_message_type type) {
    switch (type) {
        case ptp_message_type::sync:
            return "Sync";
        case ptp_message_type::delay_req:
            return "Delay_Req";
        case ptp_message_type::p_delay_req:
            return "Pdelay_Req";
        case ptp_message_type::p_delay_resp:
            return "Pdelay_Resp";
        case ptp_message_type::follow_up:
            return "Follow_Up";
        case ptp_message_type::delay_resp:
            return "Delay_resp";
        case ptp_message_type::p_delay_resp_follow_up:
            return "Pdelay_Resp_Follow_Up";
        case ptp_message_type::announce:
            return "Announce";
        case ptp_message_type::signaling:
            return "Signaling";
        case ptp_message_type::management:
            return "Management";
        case ptp_message_type::reserved1:
        case ptp_message_type::reserved2:
        case ptp_message_type::reserved3:
        case ptp_message_type::reserved4:
        case ptp_message_type::reserved5:
        case ptp_message_type::reserved6:
            return "Reserved";
        default:
            return "Unknown";
    }
}

}  // namespace rav
