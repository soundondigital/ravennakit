/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/containers/file_input_stream.hpp"

#include "ravennakit/core/exception.hpp"

rav::file_input_stream::file_input_stream(const file& f) : file_(f), fstream_(f.path(), std::ios::binary) {
    if (fstream_.fail()) {
        RAV_THROW_EXCEPTION("Failed to open file");
    }
    if (!fstream_.is_open()) {
        if (!file_.exists()) {
            RAV_THROW_EXCEPTION("File does not exist");
        }
        RAV_THROW_EXCEPTION("Failed to open file");
    }
    fstream_.exceptions (std::ofstream::failbit | std::ofstream::badbit);
    fstream_.seekg(0);
}

size_t rav::file_input_stream::read(uint8_t* buffer, const size_t size) {
    fstream_.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(size));

    // Check if reading was unsuccessful
    if (fstream_.fail() && !fstream_.eof()) {
        RAV_THROW_EXCEPTION("Failed to read from file");
    }

    const auto count = fstream_.gcount();
    if (count < 0) {
        RAV_THROW_EXCEPTION("Failed to read from file");
    }

    return static_cast<size_t>(count);
}

bool rav::file_input_stream::set_read_position(const size_t position) {
    fstream_.seekg(static_cast<std::streamsize>(position));
    return !fstream_.fail();
}

size_t rav::file_input_stream::get_read_position() {
    const auto pos = fstream_.tellg();
    if (pos == -1) {
        RAV_THROW_EXCEPTION("Failed to get read position");
    }
    return static_cast<size_t>(pos);
}

std::optional<size_t> rav::file_input_stream::size() const {
    return file_.size();
}

bool rav::file_input_stream::exhausted() const {
    return fstream_.eof();
}
