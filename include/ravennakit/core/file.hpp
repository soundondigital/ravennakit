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

#include <filesystem>
#include <fstream>

namespace rav {

/**
 * Represents a file on the file system.
 */
class file {
  public:
    file() = default;

    /**
     * Constructs a file object with the given path.
     * @param path The path to the file or directory.
     */
    explicit file(std::filesystem::path path) : path_(std::move(path)) {}

    /**
     * @returns True if the file of directory exists, or false if the file or directory does not exist.
     */
    [[nodiscard]] bool exists() const {
        return std::filesystem::exists(path_);
    }

    /**
     * Creates the file if it does not already exist.
     * @return True if the file was created or already exists, or false if the file could not be created.
     */
    [[nodiscard]] bool create_if_not_exists() const {
        if (!exists()) {
            std::ofstream f(path_);
            if (!f.good()) {
                return false; // Failed to create the file
            }
            f.close();
            if (!exists()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @return The path to the file or directory.
     */
    [[nodiscard]] const std::filesystem::path& path() const {
        return path_;
    }

    /**
     * @throws std::filesystem::filesystem_error if the file does not exist.
     * @return The size of the file in bytes.
     */
    [[nodiscard]] std::uintmax_t size() const {
        return std::filesystem::file_size(path_);
    }

  private:
    std::filesystem::path path_;
};

}  // namespace rav
