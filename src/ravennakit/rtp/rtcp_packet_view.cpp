/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtcp_packet_view.hpp"
#include "ravennakit/core/byte_order.hpp"

namespace {
constexpr size_t kHeaderLength = 8;
constexpr size_t kSenderReportNtpTimestampHalfLength = 4;
constexpr size_t kSenderReportNtpTimestampFullLength = kSenderReportNtpTimestampHalfLength * 2;
constexpr size_t kSenderReportPacketCountLength = 4;
constexpr size_t kSenderReportOctetCountLength = 4;
constexpr size_t kSenderInfoLength = kSenderReportNtpTimestampFullLength
    + rav::rtp::k_rtp_timestamp_length_length + kSenderReportPacketCountLength
    + kSenderReportOctetCountLength;
}  // namespace

rav::rtcp::PacketView::PacketView(const uint8_t* data, const size_t size_bytes) :
    data_(data), size_bytes_(size_bytes) {}

bool rav::rtcp::PacketView::validate() const {
    if (data_ == nullptr) {
        return false;
    }

    if (size_bytes_ < kHeaderLength) {
        return false;
    }

    if (version() != 2) {
        return false;
    }

    if (type() == PacketType::sender_report_report) {
        if (size_bytes_ < kHeaderLength + kSenderInfoLength) {
            return false;
        }
    }

    return true;
}

uint8_t rav::rtcp::PacketView::version() const {
    if (size_bytes_ < 1) {
        return 0;
    }
    return (data_[0] & 0b11000000) >> 6;
}

bool rav::rtcp::PacketView::padding() const {
    if (size_bytes_ < 1) {
        return false;
    }
    return (data_[0] & 0b00100000) >> 5 != 0;
}

uint8_t rav::rtcp::PacketView::reception_report_count() const {
    if (size_bytes_ < 1) {
        return false;
    }
    return data_[0] & 0b00011111;
}

rav::rtcp::PacketView::PacketType rav::rtcp::PacketView::type() const {
    if (size_bytes_ < 2) {
        return PacketType::unknown;
    }

    switch (data_[1]) {
        case 200:
            return PacketType::sender_report_report;
        case 201:
            return PacketType::receiver_report_report;
        case 202:
            return PacketType::source_description_items_items;
        case 203:
            return PacketType::bye;
        case 204:
            return PacketType::app;
        default:
            return PacketType::unknown;
    }
}

uint16_t rav::rtcp::PacketView::length() const {
    constexpr auto kOffset = 2;
    if (size_bytes_ < kOffset + sizeof(uint16_t)) {
        return 0;
    }

    return read_be<uint16_t>(&data_[kOffset]) + 1;
}

uint32_t rav::rtcp::PacketView::ssrc() const {
    constexpr auto kOffset = 4;
    if (size_bytes_ < kOffset + sizeof(uint32_t)) {
        return 0;
    }
    return read_be<uint32_t>(&data_[kOffset]);
}

rav::ntp::Timestamp rav::rtcp::PacketView::ntp_timestamp() const {
    if (type() != PacketType::sender_report_report) {
        return {};
    }

    if (size_bytes_ < kHeaderLength + kSenderReportNtpTimestampFullLength) {
        return {};
    }

    return {
        read_be<uint32_t>(data_ + kHeaderLength),
        read_be<uint32_t>(data_ + kHeaderLength + kSenderReportNtpTimestampHalfLength)
    };
}

uint32_t rav::rtcp::PacketView::rtp_timestamp() const {
    if (type() != PacketType::sender_report_report) {
        return {};
    }

    constexpr auto offset = kHeaderLength + kSenderReportNtpTimestampFullLength;
    if (size_bytes_ < offset + rtp::k_rtp_timestamp_length_length) {
        return {};
    }

    return read_be<uint32_t>(data_ + offset);
}

uint32_t rav::rtcp::PacketView::packet_count() const {
    if (type() != PacketType::sender_report_report) {
        return {};
    }

    constexpr auto offset = kHeaderLength + kSenderReportNtpTimestampFullLength + rtp::k_rtp_timestamp_length_length;
    if (size_bytes_ < offset + kSenderReportPacketCountLength) {
        return {};
    }

    return read_be<uint32_t>(data_ + offset);
}

uint32_t rav::rtcp::PacketView::octet_count() const {
    if (type() != PacketType::sender_report_report) {
        return {};
    }

    constexpr auto offset =
        kHeaderLength + kSenderReportNtpTimestampFullLength + rtp::k_rtp_timestamp_length_length + kSenderReportOctetCountLength;
    if (size_bytes_ < offset + kSenderReportOctetCountLength) {
        return {};
    }

    return read_be<uint32_t>(data_ + offset);
}

rav::rtcp::ReportBlockView rav::rtcp::PacketView::get_report_block(const size_t index) const {
    if (index >= reception_report_count()) {
        return {};
    }

    auto offset = kHeaderLength;

    if (type() == PacketType::sender_report_report) {
        offset += kSenderInfoLength;
    }

    if (size_bytes_ < offset + ReportBlockView::k_report_block_length_length * (index + 1)) {
        return {};
    }

    return {
        data_ + offset + ReportBlockView::k_report_block_length_length * index,
        ReportBlockView::k_report_block_length_length
    };
}

rav::buffer_view<const uint8_t> rav::rtcp::PacketView::get_profile_specific_extension() const {
    if (data_ == nullptr) {
        return {};
    }

    auto offset = kHeaderLength + ReportBlockView::k_report_block_length_length * reception_report_count();

    if (type() == PacketType::sender_report_report) {
        offset += kSenderInfoLength;
    }

    const auto reported_length = static_cast<size_t>(length()) * 4;

    if (offset >= size_bytes_) {
        return {};
    }

    if (reported_length > size_bytes_) {
        return {};
    }

    return {data_ + offset, reported_length - offset};
}

rav::rtcp::PacketView rav::rtcp::PacketView::get_next_packet() const {
    if (data_ == nullptr) {
        return {};
    }
    const auto reported_length = static_cast<size_t>(length()) * 4;
    if (reported_length >= size_bytes_) {
        return {};
    }
    return {data_ + reported_length, size_bytes_ - reported_length};
}

const uint8_t* rav::rtcp::PacketView::data() const {
    return data_;
}

size_t rav::rtcp::PacketView::size() const {
    return size_bytes_;
}

std::string rav::rtcp::PacketView::to_string() const {
    auto header = fmt::format(
        "RTCP Packet valid={} | Header version={} padding={} reception_report_count={} packet_type={} length={} ssrc={}",
        validate(), version(), padding(), reception_report_count(), packet_type_to_string(type()), length(), ssrc()
    );

    if (type() == PacketType::sender_report_report) {
        return fmt::format(
            "{} | Sender info ntp={} rtp={} packet_count={} octet_count={}", header, ntp_timestamp().to_string(),
            rtp_timestamp(), packet_count(), octet_count()
        );
    }

    return header;
}

const char* rav::rtcp::PacketView::packet_type_to_string(const PacketType packet_type) {
    switch (packet_type) {
        case PacketType::sender_report_report:
            return "SenderReport";
        case PacketType::receiver_report_report:
            return "ReceiverReport";
        case PacketType::source_description_items_items:
            return "SourceDescriptionItems";
        case PacketType::bye:
            return "Bye";
        case PacketType::app:
            return "App";
        case PacketType::unknown:
            return "Unknown";
        default:
            return "";
    }
}
