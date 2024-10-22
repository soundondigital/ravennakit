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

#include <cstdint>

namespace rav {

class output_stream {
public:
    output_stream() = default;
    virtual ~output_stream() = default;

    /**
     * Writes data from the given buffer to the stream.
     * @param buffer The buffer to write data from.
     * @param size The number of bytes to write.
     * @return The number of bytes written.
     */
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;

    /**
     * Sets the write position in the stream.
     * @param position The new write position.
     * @return True if the write position was successfully set.
     */
    virtual bool set_write_position(size_t position) = 0;

    /**
     * @return The current write position in the stream.
     */
    [[nodiscard]] virtual size_t get_write_position() const = 0;

    /**
    * Flushes the stream, ensuring that all data is written to the underlying storage. Not all streams support this
    * operation.
    */
    virtual void flush() = 0;
};

}
