/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtp_receiver.hpp"

#include <fmt/core.h>

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/rollback.hpp"
#include "ravennakit/rtp/rtcp_packet_view.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

rav::rtp_receiver::~rtp_receiver() {
    const auto result = close();
    if (result.holds_error()) {
        RAV_ERROR("Error: {}", result.what());
    }
}

rav::result rav::rtp_receiver::bind(const std::string& address, const uint16_t port, const udp_flags opts) {
    const auto result = stop();

    if (result.holds_error()) {
        return result;
    }

    auto rtp_socket = loop_->resource<uvw::udp_handle>();
    if (rtp_socket == nullptr) {
        return RESULT(error::resource_failure);
    }

    rtp_socket->on<uvw::udp_data_event>(
        [this](const uvw::udp_data_event& event, [[maybe_unused]] uvw::udp_handle& handle) {
            const rav::rtp_packet_view rtp_packet(reinterpret_cast<const uint8_t*>(event.data.get()), event.length);
            publish(rtp_packet_event {rtp_packet});
        }
    );

    rtp_socket->on<uvw::error_event>([](const uvw::error_event& event, uvw::udp_handle& handle) {
        RAV_ERROR("Error: {}", event.what());
        handle.close();
    });

    {
        const uvw::error_event error(rtp_socket->bind(address, port, opts));
        if (error) {
            return RESULT(error);
        }
    }

    auto rtcp_socket = loop_->resource<uvw::udp_handle>();
    if (rtcp_socket == nullptr) {
        return RESULT(error::resource_failure);
    }

    rtcp_socket->on<uvw::udp_data_event>([](const uvw::udp_data_event& event,
                                            [[maybe_unused]] uvw::udp_handle& handle) {
        const rav::rtcp_packet_view rtcp_packet(reinterpret_cast<const uint8_t*>(event.data.get()), event.length);
        fmt::println("{}", rtcp_packet.to_string());
    });

    rtcp_socket->on<uvw::error_event>([](const uvw::error_event& event, uvw::udp_handle& handle) {
        RAV_ERROR("Error: {}", event.what());
        handle.close();
    });

    {
        const uvw::error_event error(rtcp_socket->bind(address, port + 1, opts));
        if (error) {
            return RESULT(error);
        }
    }

    rtp_socket_ = std::move(rtp_socket);
    rtcp_socket_ = std::move(rtcp_socket);

    return ok();
}

rav::result rav::rtp_receiver::start() const {
    if (rtp_socket_ == nullptr || rtcp_socket_ == nullptr) {
        return RESULT(error::invalid_state);
    }

    const uvw::error_event rtp_error(rtp_socket_->recv());
    if (rtp_error) {
        std::ignore = stop();
        return RESULT(rtp_error);
    }

    const uvw::error_event rtcp_error(rtcp_socket_->recv());
    if (rtcp_error) {
        std::ignore = stop();
        return RESULT(rtcp_error);
    }

    return ok();
}

rav::result rav::rtp_receiver::stop() const {
    result result;

    if (rtp_socket_ != nullptr) {
        const uvw::error_event rtp_error(rtp_socket_->stop());
        if (rtp_error) {
            result = rtp_error; // TODO: Use RESULT macro
        }
    }

    if (rtcp_socket_ != nullptr) {
        const uvw::error_event rtcp_error(rtcp_socket_->stop());
        if (rtcp_error && result.is_ok()) {
            result = rtcp_error; // TODO: Use RESULT macro
        }
    }

    return result;
}

rav::result rav::rtp_receiver::close() {
    const auto result = stop();

    if (rtp_socket_ != nullptr) {
        rtp_socket_->close();
        rtp_socket_->reset();
        rtp_socket_.reset();
    }

    if (rtcp_socket_ != nullptr) {
        rtcp_socket_->close();
        rtcp_socket_->reset();
        rtcp_socket_.reset();
    }

    return result;
}
