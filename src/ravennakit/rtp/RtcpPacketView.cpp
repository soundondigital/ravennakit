/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/RtcpPacketView.hpp"

#include <fmt/core.h>

#include "ravennakit/platform/ByteOrder.hpp"

namespace {
constexpr size_t kHeaderLength = 8;
constexpr size_t kSenderReportNtpTimestampHalfLength = 4;
constexpr size_t kSenderReportNtpTimestampFullLength = kSenderReportNtpTimestampHalfLength * 2;
constexpr size_t kSenderReportPacketCountLength = 4;
constexpr size_t kSenderReportOctetCountLength = 4;
constexpr size_t kSenderInfoLength = kSenderReportNtpTimestampFullLength + rav::rtp::kRtpTimestampLength
    + kSenderReportPacketCountLength + kSenderReportOctetCountLength;
}  // namespace

rav::RtcpPacketView::RtcpPacketView(const uint8_t* data, const size_t data_length) :
    data_(data), data_length_(data_length) {}

rav::rtp::Result rav::RtcpPacketView::validate() const {
    if (data_ == nullptr) {
        return rtp::Result::InvalidPointer;
    }

    if (data_length_ < kHeaderLength) {
        return rtp::Result::InvalidHeaderLength;
    }

    if (version() != 2) {
        return rtp::Result::InvalidVersion;
    }

    if (packet_type() == PacketType::SenderReport) {
        if (data_length_ < kHeaderLength + kSenderInfoLength) {
            return rtp::Result::InvalidSenderInfoLength;
        }
    }

    return rtp::Result::Ok;
}

uint8_t rav::RtcpPacketView::version() const {
    if (data_length_ < 1) {
        return 0;
    }
    return (data_[0] & 0b11000000) >> 6;
}

bool rav::RtcpPacketView::padding() const {
    if (data_length_ < 1) {
        return false;
    }
    return (data_[0] & 0b00100000) >> 5 != 0;
}

uint8_t rav::RtcpPacketView::reception_report_count() const {
    if (data_length_ < 1) {
        return false;
    }
    return data_[0] & 0b00011111;
}

rav::RtcpPacketView::PacketType rav::RtcpPacketView::packet_type() const {
    if (data_length_ < 2) {
        return PacketType::Unknown;
    }

    switch (data_[1]) {
        case 200:
            return PacketType::SenderReport;
        case 201:
            return PacketType::ReceiverReport;
        case 202:
            return PacketType::SourceDescriptionItems;
        case 203:
            return PacketType::Bye;
        case 204:
            return PacketType::App;
        default:
            return PacketType::Unknown;
    }
}

uint16_t rav::RtcpPacketView::length() const {
    constexpr auto kOffset = 2;
    if (data_length_ < kOffset + sizeof(uint16_t)) {
        return 0;
    }

    return byte_order::read_be<uint16_t>(&data_[kOffset]) + 1;
}

uint32_t rav::RtcpPacketView::ssrc() const {
    constexpr auto kOffset = 4;
    if (data_length_ < kOffset + sizeof(uint32_t)) {
        return 0;
    }
    return byte_order::read_be<uint32_t>(&data_[kOffset]);
}

rav::ntp::Timestamp rav::RtcpPacketView::ntp_timestamp() const {
    if (packet_type() != PacketType::SenderReport) {
        return {};
    }

    if (data_length_ < kHeaderLength + kSenderReportNtpTimestampFullLength) {
        return {};
    }

    return {
        byte_order::read_be<uint32_t>(data_ + kHeaderLength),
        byte_order::read_be<uint32_t>(data_ + kHeaderLength + kSenderReportNtpTimestampHalfLength)
    };
}

uint32_t rav::RtcpPacketView::rtp_timestamp() const {
    if (packet_type() != PacketType::SenderReport) {
        return {};
    }

    constexpr auto offset = kHeaderLength + kSenderReportNtpTimestampFullLength;
    if (data_length_ < offset + rtp::kRtpTimestampLength) {
        return {};
    }

    return byte_order::read_be<uint32_t>(data_ + offset);
}

uint32_t rav::RtcpPacketView::packet_count() const {
    if (packet_type() != PacketType::SenderReport) {
        return {};
    }

    constexpr auto offset = kHeaderLength + kSenderReportNtpTimestampFullLength + rtp::kRtpTimestampLength;
    if (data_length_ < offset + kSenderReportPacketCountLength) {
        return {};
    }

    return byte_order::read_be<uint32_t>(data_ + offset);
}

uint32_t rav::RtcpPacketView::octet_count() const {
    if (packet_type() != PacketType::SenderReport) {
        return {};
    }

    constexpr auto offset =
        kHeaderLength + kSenderReportNtpTimestampFullLength + rtp::kRtpTimestampLength + kSenderReportOctetCountLength;
    if (data_length_ < offset + kSenderReportOctetCountLength) {
        return {};
    }

    return byte_order::read_be<uint32_t>(data_ + offset);
}

rav::RtcpReportBlockView rav::RtcpPacketView::get_report_block(const size_t index) const {
    if (index >= reception_report_count()) {
        return {};
    }

    auto offset = kHeaderLength;

    if (packet_type() == PacketType::SenderReport) {
        offset += kSenderInfoLength;
    }

    if (data_length_ < offset + RtcpReportBlockView::kReportBlockLength * (index + 1)) {
        return {};
    }

    return {data_ + offset + RtcpReportBlockView::kReportBlockLength * index, RtcpReportBlockView::kReportBlockLength};
}

rav::BufferView<const uint8_t> rav::RtcpPacketView::get_profile_specific_extension() const {
    if (data_ == nullptr) {
        return {};
    }

    auto offset = kHeaderLength + RtcpReportBlockView::kReportBlockLength * reception_report_count();

    if (packet_type() == PacketType::SenderReport) {
        offset += kSenderInfoLength;
    }

    const auto reported_length = static_cast<size_t>(length()) * 4;

    if (offset >= data_length_) {
        return {};
    }

    if (reported_length > data_length_) {
        return {};
    }

    return {data_ + offset, reported_length - offset};
}

rav::RtcpPacketView rav::RtcpPacketView::get_next_packet() const {
    if (data_ == nullptr) {
        return {};
    }
    const auto reported_length = static_cast<size_t>(length()) * 4;
    if (reported_length >= data_length_) {
        return {};
    }
    return {data_ + reported_length, data_length_ - reported_length};
}

bool rav::RtcpPacketView::is_valid() const {
    return data_ != nullptr;
}

const uint8_t* rav::RtcpPacketView::data() const {
    return data_;
}

size_t rav::RtcpPacketView::data_length() const {
    return data_length_;
}

std::string rav::RtcpPacketView::to_string() const {
    auto header = fmt::format(
        "RTCP Packet valid={} | Header version={} padding={} reception_report_count={} packet_type={} length={} ssrc={}",
        validate() == rtp::Result::Ok, version(), padding(), reception_report_count(),
        packet_type_to_string(packet_type()), length(), ssrc()
    );

    if (packet_type() == PacketType::SenderReport) {
        return fmt::format(
            "{} | Sender info ntp={} rtp={} packet_count={} octet_count={}", header, ntp_timestamp().to_string(),
            rtp_timestamp(), packet_count(), octet_count()
        );
    }

    return header;
}

const char* rav::RtcpPacketView::packet_type_to_string(const PacketType packet_type) {
    switch (packet_type) {
        case PacketType::SenderReport:
            return "SenderReport";
        case PacketType::ReceiverReport:
            return "ReceiverReport";
        case PacketType::SourceDescriptionItems:
            return "SourceDescriptionItems";
        case PacketType::Bye:
            return "Bye";
        case PacketType::App:
            return "App";
        case PacketType::Unknown:
            return "Unknown";
        default:
            return "";
    }
}
