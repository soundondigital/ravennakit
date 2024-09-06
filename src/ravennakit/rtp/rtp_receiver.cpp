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
#include "ravennakit/uv/uv_exception.hpp"

rav::rtp_receiver::rtp_receiver(const std::shared_ptr<uvw::loop>& loop) :
    loop_(loop), rtp_socket_(loop_->resource<uvw::udp_handle>()), rtcp_socket_(loop_->resource<uvw::udp_handle>()) {
    rtp_socket_->on<uvw::udp_data_event>(
        [this](const uvw::udp_data_event& event, [[maybe_unused]] uvw::udp_handle& handle) {
            // This is the last point where exceptions can be caught before going back into c-land and crashing
            try {
                const rtp_packet_view rtp_packet(reinterpret_cast<const uint8_t*>(event.data.get()), event.length);
                publish(rtp_packet_event {rtp_packet});
            }
            CATCH_LOG_UNCAUGHT_EXCEPTIONS
        }
    );

    rtp_socket_->on<uvw::error_event>([this](const uvw::error_event& event, uvw::udp_handle& handle) {
        RAV_ERROR("Error: {}", event.what());
        close();
    });

    rtcp_socket_->on<uvw::udp_data_event>(
        [this](const uvw::udp_data_event& event, [[maybe_unused]] uvw::udp_handle& handle) {
            // This is the last point where exception can be caught before going back into c-land and crashing
            try {
                const rtcp_packet_view rtcp_packet(reinterpret_cast<const uint8_t*>(event.data.get()), event.length);
                publish(rtcp_packet_event {rtcp_packet});
            }
            CATCH_LOG_UNCAUGHT_EXCEPTIONS
        }
    );

    rtcp_socket_->on<uvw::error_event>([this](const uvw::error_event& event, uvw::udp_handle& handle) {
        RAV_ERROR("Error: {}", event.what());
        close();
    });
}

rav::rtp_receiver::~rtp_receiver() {
    rtp_socket_->reset();
    rtcp_socket_->reset();
}

void rav::rtp_receiver::bind(const std::string& address, const uint16_t port, const udp_flags opts) const {
    UV_THROW_IF_ERROR(rtp_socket_->bind(address, port, opts));
    rollback rollback([this] {
        const uvw::error_event err(rtp_socket_->stop());
        RAV_ERROR("Error: {}", err.what());
    });

    const auto [ip, bound_port] = rtp_socket_->sock();
    if (bound_port == 0 || ip.empty()) {
        RAV_THROW_EXCEPTION("failed to bind to port");
    }

    UV_THROW_IF_ERROR(rtcp_socket_->bind(address, bound_port + 1, opts));

    rollback.commit();
}

void rav::rtp_receiver::set_multicast_membership(
    const std::string& multicast_address, const std::string& interface_address,
    const uvw::udp_handle::membership membership
) const {
    const auto r1 = rtp_socket_->multicast_membership(multicast_address, interface_address, membership);
    const auto r2 = rtcp_socket_->multicast_membership(multicast_address, interface_address, membership);

    if (!r1 && !r2) {
        RAV_THROW_EXCEPTION("failed to join multicast group on both rtp and rtcp socket");
    }
    if (!r1) {
        RAV_THROW_EXCEPTION("failed to join multicast group on rtp socket");
    }
    if (!r2) {
        RAV_THROW_EXCEPTION("failed to join multicast group on rtcp socket");
    }
}

void rav::rtp_receiver::start() const {
    UV_THROW_IF_ERROR(rtp_socket_->recv());

    rollback rollback([this] {
        const uvw::error_event err(rtp_socket_->stop());
        RAV_ERROR("Error: {}", err.what());
    });

    UV_THROW_IF_ERROR(rtcp_socket_->recv())

    rollback.commit();
}

void rav::rtp_receiver::stop() const {
    int r1 {}, r2 {};

    if (rtp_socket_ != nullptr) {
        r1 = rtp_socket_->stop();
    }

    if (rtcp_socket_ != nullptr) {
        r2 = rtcp_socket_->stop();
    }

    UV_THROW_IF_ERROR(r1);
    UV_THROW_IF_ERROR(r2);
}

void rav::rtp_receiver::close() const {
    if (rtp_socket_ != nullptr) {
        rtp_socket_->close();
    }

    if (rtcp_socket_ != nullptr) {
        rtcp_socket_->close();
    }
}
