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

#include "input_stream.hpp"
#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/file.hpp"

namespace rav {

/**
 * An implementation of input_stream for reading from a file.
 */
class FileInputStream final: public InputStream {
  public:
    explicit FileInputStream(const std::filesystem::path& f) {
        fstream_.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fstream_.open(f, std::ios::binary | std::ios::ate);
        if (!fstream_.is_open()) {
            if (!std::filesystem::exists(f)) {
                RAV_THROW_EXCEPTION("File does not exist");
            }
            RAV_THROW_EXCEPTION("Failed to open file");
        }
        const auto file_size = fstream_.tellg();
        RAV_ASSERT(file_size != -1, "Failed to get file size");
        file_size_ = static_cast<size_t>(fstream_.tellg());
        fstream_.seekg(0);

        RAV_ASSERT(std::filesystem::file_size(f) == file_size_, "File reports a different size than the stream");
    }

    // input_stream overrides
    [[nodiscard]] tl::expected<size_t, Error> read(uint8_t* buffer, const size_t size) override {
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

    bool set_read_position(const size_t position) override {
        fstream_.seekg(static_cast<std::streamsize>(position));
        return !fstream_.fail();
    }

    [[nodiscard]] size_t get_read_position() override {
        const auto pos = fstream_.tellg();
        if (pos == -1) {
            RAV_THROW_EXCEPTION("Failed to get read position");
        }
        return static_cast<size_t>(pos);
    }

    [[nodiscard]] std::optional<size_t> size() const override {
        return file_size_;
    }

    [[nodiscard]] bool exhausted() override {
        return fstream_.eof() || fstream_.peek() == std::ifstream::traits_type::eof();
    }

  private:
    std::ifstream fstream_;
    size_t file_size_;
};

}  // namespace rav
