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

#include "ravennakit/core/byte_order.hpp"
#include "ravennakit/ptp/ptp_definitions.hpp"

#include <cstdint>
#include <tl/expected.hpp>

namespace rav {

class ptp_message_header {
  public:
    enum class error { not_enough_data, invalid_data_length };

    tl::expected<void, error> decode(const uint8_t* data, const size_t size_bytes) {
        if (size_bytes < k_header_size) {
            return tl::unexpected(error::not_enough_data);
        }

        message_length_ = rav::byte_order::read_be<uint16_t>(data + 2);
        if (message_length_ != size_bytes) {
            return tl::unexpected(error::invalid_data_length);
        }

        sdo_id_ = static_cast<uint16_t>((data[0] & 0b11110000) << 4 | data[5]);

        message_type_ = static_cast<ptp_message_type>(data[0] & 0b00001111);
        version_ptp_ = data[1] | 0b00001111;
        minor_version_ptp_ = (data[1] | 0b11110000) >> 4;

        return {};
    }

    [[nodiscard]] uint16_t sdo_id() const {
        return sdo_id_;
    }

    [[nodiscard]] std::string to_string() const {
        return fmt::format("PTP {}: sdo_id={} version={}.{}", ptp_message_type_to_string(message_type_), sdo_id_, version_ptp_, minor_version_ptp_);
    }

  private:
    constexpr static size_t k_header_size = 34;
    uint16_t sdo_id_ {};
    ptp_message_type message_type_ {};
    uint16_t message_length_ {};
    uint8_t version_ptp_{};
    uint8_t minor_version_ptp_{};
};

}  // namespace rav
