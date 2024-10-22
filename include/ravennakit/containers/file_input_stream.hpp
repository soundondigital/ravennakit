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
#include "ravennakit/core/file.hpp"

namespace rav {

class file_input_stream final: public input_stream {
public:
    explicit file_input_stream(const file& f);

    // input_stream overrides
    size_t read(uint8_t* buffer, size_t size) override;
    bool set_read_position(size_t position) override;
    [[nodiscard]] size_t get_read_position() override;
    [[nodiscard]] std::optional<size_t> size() const override;
    [[nodiscard]] bool exhausted() const override;

private:
    const file file_;
    std::ifstream fstream_;
};

}
