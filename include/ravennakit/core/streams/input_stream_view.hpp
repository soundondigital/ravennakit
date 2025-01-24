/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "input_stream.hpp"

namespace rav {

/**
 * A non-owning view of some data that can be read from.
 */
class input_stream_view: public input_stream {
  public:
    /**
     * Constructs a new input stream view pointing to the given data. It doesn't take ownership of the data so make sure
     * that the data outlives the stream.
     * @param data The data to read from. Must not be nullptr.
     * @param size The size of the data.
     */
    input_stream_view(const uint8_t* data, size_t size);

    /**
     * Constructs a new input stream view pointing to the given container. It doesn't take ownership of the container so
     * make sure that the container outlives the stream.
     * @tparam T The container type.
     * @param container The container to read from. Must not be empty.
     */
    template<class T>
    explicit input_stream_view(const T& container) : input_stream_view(container.data(), container.size()) {}

    ~input_stream_view() override = default;

    input_stream_view(const input_stream_view&) = default;
    input_stream_view& operator=(const input_stream_view&) = default;

    input_stream_view(input_stream_view&&) noexcept = default;
    input_stream_view& operator=(input_stream_view&&) noexcept = default;

    /**
     * Resets the stream to its initial state by setting the read position to 0.
     */
    void reset();

    // input_stream overrides
    [[nodiscard]] tl::expected<size_t, error> read(uint8_t* buffer, size_t size) override;
    [[nodiscard]] bool set_read_position(size_t position) override;
    [[nodiscard]] size_t get_read_position() override;
    [[nodiscard]] std::optional<size_t> size() const override;
    [[nodiscard]] bool exhausted() override;

  private:
    const uint8_t* data_{};
    size_t size_ {};
    size_t read_position_ = 0;
};

}  // namespace rav
