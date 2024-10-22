/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/containers/input_stream.hpp"

std::optional<size_t> rav::input_stream::remaining() {
    if (const auto s = size()) {
        return *s - get_read_position();
    }
    return std::nullopt;
}

bool rav::input_stream::skip(const size_t size) {
    return set_read_position(get_read_position() + size);
}

std::string rav::input_stream::read_as_string(const size_t size) {
    std::string str(size, '\0');
    const auto n = read(reinterpret_cast<uint8_t*>(str.data()), size);
    str.resize(n);
    return str;
}
