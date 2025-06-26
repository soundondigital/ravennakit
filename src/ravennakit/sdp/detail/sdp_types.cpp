/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_types.hpp"

#include "ravennakit/sdp/detail/sdp_constants.hpp"

const char* rav::sdp::to_string(const NetwType& type) {
    switch (type) {
        case NetwType::internet:
            return k_sdp_inet;
        case NetwType::undefined:
        default:
            return "undefined";
    }
}

const char* rav::sdp::to_string(const AddrType& type) {
    switch (type) {
        case AddrType::ipv4:
            return k_sdp_ipv4;
        case AddrType::ipv6:
            return k_sdp_ipv6;
        case AddrType::both:
            return k_sdp_wildcard;
        case AddrType::undefined:
        default:
            return "undefined";
    }
}

const char* rav::sdp::to_string(const MediaDirection& direction) {
    switch (direction) {
        case MediaDirection::sendrecv:
            return k_sdp_sendrecv;
        case MediaDirection::sendonly:
            return k_sdp_sendonly;
        case MediaDirection::recvonly:
            return k_sdp_recvonly;
        case MediaDirection::inactive:
            return k_sdp_inactive;
        default:
            return "undefined";
    }
}

const char* rav::sdp::to_string(const FilterMode& filter_mode) {
    switch (filter_mode) {
        case FilterMode::exclude:
            return "excl";
        case FilterMode::include:
            return "incl";
        case FilterMode::undefined:
        default:
            return "undefined";
    }
}
