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

namespace rav::sdp {

enum class NetwType { undefined, internet };
enum class AddrType { undefined, ipv4, ipv6, both };
enum class MediaDirection { sendrecv, sendonly, recvonly, inactive };
enum class FilterMode { undefined, exclude, include };

const char* to_string(const NetwType& type);
const char* to_string(const AddrType& type);
const char* to_string(const MediaDirection& direction);
const char* to_string(const FilterMode& filter_mode);

}
