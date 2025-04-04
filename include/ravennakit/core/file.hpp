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
class File {
  public:
    File() = default;

    /**
     * Constructs a file object with the given path.
     * @param path The path to the file or directory.
     */
    explicit File(std::filesystem::path path) : path_(std::move(path)) {}

    /**
     * Constructs a file object with the given path.
     * @param path The path to the file or directory.
     */
    explicit File(const char* path) : path_(path) {}

    /**
     * Appends given path to the file path.
     * @param p The path to append.
     * @return A new file object with the appended path.
     */
    File& operator/(const std::filesystem::path& p) {
        return *this /= p;
    }

    /**
     * Appends given path to the file path.
     * @param p The path to append.
     * @return A new file object with the appended path.
     */
    File& operator/=(const std::filesystem::path& p) {
        path_ /= p;
        return *this;
    }

    /**
     * @returns True if the file of directory exists, or false if the file or directory does not exist.
     */
    [[nodiscard]] bool exists() const {
        return std::filesystem::exists(path_);
    }

    /**
     * Creates the file if it does not already exist.
     * @return True if the file was created or already existed, or false if the file could not be created.
     */
    [[nodiscard]] bool create_if_not_exists() const {
        if (!exists()) {
            std::ofstream f(path_);
            if (!f.good()) {
                return false;  // Failed to create the file
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
     * @return The parent directory of the file.
     */
    [[nodiscard]] File parent() const {
        return File(path_.parent_path());
    }

    /**
     * @return The absolute path to the file.
     */
    [[nodiscard]] File absolute() const {
        return File(std::filesystem::absolute(path_));
    }

    /**
     * @throws std::filesystem::filesystem_error if the file does not exist.
     * @return The size of the file in bytes.
     */
    [[nodiscard]] std::uintmax_t size() const {
        return std::filesystem::file_size(path_);
    }

    /**
     * @return Path as a string.
     */
    [[nodiscard]] std::string to_string() const {
        return path_.string();
    }

  private:
    std::filesystem::path path_;
};

}  // namespace rav
