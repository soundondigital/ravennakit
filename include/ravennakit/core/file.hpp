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

#include "ravennakit/core/expected.hpp"

#include <filesystem>
#include <fstream>

namespace rav::file {

enum class Error {
    invalid_path,
    file_does_not_exist,
    failed_to_open,
    failed_to_get_file_size,
    failed_to_read_from_file,
};

/**
 * Creates the file if it does not already exist.
 * @return True if the file was created or already existed, or false if the file could not be created.
 */
[[nodiscard]] inline bool create_if_not_exists(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::ofstream f(path);
        if (!f.good()) {
            return false;  // Failed to create the file
        }
        f.close();
        if (!std::filesystem::exists(path)) {
            return false;
        }
    }
    return true;
}

/**
 * Reads the contents of given file into a string.
 * @param file The file to read from.
 * @return An expected holding a string with the contents on success, or an Error in case of failure.
 */
inline tl::expected<std::string, Error> read_file_as_string(const std::filesystem::path& file) {
    if (file.empty()) {
        return tl::unexpected(Error::invalid_path);
    }

    std::ifstream stream;

    stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    stream.open(file, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        if (!std::filesystem::exists(file)) {
            return tl::unexpected(Error::file_does_not_exist);
        }
        return tl::unexpected(Error::failed_to_open);
    }

    const auto file_size = stream.tellg();
    if (file_size < 0) {
        return tl::unexpected(Error::failed_to_get_file_size);
    }

    stream.seekg(0);

    RAV_ASSERT(std::filesystem::file_size(file) == static_cast<uintmax_t>(file_size), "File reports a different size than the stream");

    std::string result(static_cast<size_t>(file_size), '\0');
    stream.read(result.data(), file_size);

    if (stream.fail() && !stream.eof()) {
        return tl::unexpected(Error::failed_to_read_from_file);
    }

    const auto count = stream.gcount();
    if (count < 0) {
        return tl::unexpected(Error::failed_to_read_from_file);
    }

    if (count != file_size) {
        return tl::unexpected(Error::failed_to_read_from_file);
    }

    return result;
}

}  // namespace rav::file
