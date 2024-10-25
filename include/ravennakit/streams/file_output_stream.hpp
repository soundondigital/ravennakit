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
#include "output_stream.hpp"
#include "ravennakit/core/file.hpp"

namespace rav {

/**
 * An implementation of output_stream for writing to a file.
 */
class file_output_stream final: public output_stream {
  public:
    explicit file_output_stream(const file& file) {
        ofstream_.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        ofstream_.open(file.path(), std::ios::binary);
        if (!ofstream_.is_open()) {  // Note: not sure if this check is necessary
            RAV_THROW_EXCEPTION("Failed to open file");
        }
        ofstream_.seekp(0);
    }

    ~file_output_stream() override = default;

    // output_stream overrides
    size_t write(const uint8_t* buffer, size_t size) override {
        ofstream_.write(reinterpret_cast<const char*>(buffer), static_cast<std::streamsize>(size));
        return size;
    }

    bool set_write_position(const size_t position) override {
        ofstream_.seekp(static_cast<std::streamsize>(position));
        return false;
    }

    [[nodiscard]] size_t get_write_position() override {
        return static_cast<size_t>(ofstream_.tellp());
    }

    void flush() override {
        ofstream_.flush();
    }

  private:
    std::ofstream ofstream_;
};

}  // namespace rav
