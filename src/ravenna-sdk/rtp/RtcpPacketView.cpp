/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravenna-sdk/rtp/RtcpPacketView.hpp"

#include <fmt/core.h>

#include "ravenna-sdk/platform/ByteOrder.hpp"

namespace {
constexpr auto kHeaderLength = 8;
constexpr auto kSenderReportNtpTimestampHalfLength = 4;
constexpr auto kSenderReportNtpTimestampFullLength = kSenderReportNtpTimestampHalfLength * 2;
constexpr auto kSenderReportPacketCountLength = 4;
constexpr auto kSenderReportOctetCountLength = 4;
constexpr auto kSenderInfoLength = kSenderReportNtpTimestampFullLength + rav::rtp::kRtpTimestampLength
    + kSenderReportPacketCountLength + kSenderReportOctetCountLength;
constexpr auto kSenderReportMinLength = kHeaderLength + kSenderInfoLength;
}  // namespace

rav::RtcpPacketView::RtcpPacketView(const uint8_t* data, const size_t data_length) :
    data_(data), data_length_(data_length) {}

rav::rtp::Result rav::RtcpPacketView::verify() const {
    if (data_ == nullptr) {
        return rtp::Result::InvalidPointer;
    }

    if (data_length_ < kHeaderLength) {
        return rtp::Result::InvalidHeaderLength;
    }

    if (version() > 2) {
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

    if (data_length_ < kSenderReportMinLength + RtcpReportBlockView::kReportBlockLength * (index + 1)) {
        return {};
    }

    return {
        data_ + kSenderReportMinLength + RtcpReportBlockView::kReportBlockLength * index,
        RtcpReportBlockView::kReportBlockLength
    };
}

std::string rav::RtcpPacketView::to_string() const {
    auto header = fmt::format(
        "RTCP Packet: valid={} version={} padding={} reception_report_count={} packet_type={} length={} ssrc={}",
        verify() == rtp::Result::Ok, version(), padding(), reception_report_count(),
        packet_type_to_string(packet_type()), length(), ssrc()
    );

    if (packet_type() == PacketType::SenderReport) {}

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
    }
    return "";
}
