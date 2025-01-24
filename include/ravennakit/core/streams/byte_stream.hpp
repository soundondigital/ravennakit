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
#include "output_stream.hpp"

#include <vector>

namespace rav {

/**
 * Simple stream implementation that writes to and reads from a vector.
 */
class byte_stream final: public input_stream, public output_stream {
  public:
    byte_stream() = default;
    explicit byte_stream(std::vector<uint8_t> data);

    /**
     * Resets the stream to its initial state by clearing the data and setting the read and write positions to 0.
     */
    void reset();

    // input_stream overrides
    [[nodiscard]] tl::expected<size_t, input_stream::error> read(uint8_t* buffer, size_t size) override;
    [[nodiscard]] bool set_read_position(size_t position) override;
    [[nodiscard]] size_t get_read_position() override;
    [[nodiscard]] std::optional<size_t> size() const override;
    [[nodiscard]] bool exhausted() override;

    // output_stream overrides
    [[nodiscard]] tl::expected<void, output_stream::error> write(const uint8_t* buffer, size_t size) override;
    [[nodiscard]] tl::expected<void, output_stream::error> set_write_position(size_t position) override;
    [[nodiscard]] size_t get_write_position() override;
    void flush() override;

  private:
    std::vector<uint8_t> data_;
    size_t read_position_ = 0;
    size_t write_position_ = 0;
};

}  // namespace rav
