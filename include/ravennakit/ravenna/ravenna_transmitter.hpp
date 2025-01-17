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

#include <utility>

#include "ravennakit/core/uri.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

class ravenna_transmitter {
  public:
    ravenna_transmitter(
        dnssd::dnssd_advertiser& advertiser, rtsp_server& rtsp_server, const id id, std::string session_name,
        asio::ip::address_v4 interface_address
    ) :
        advertiser_(advertiser),
        rtsp_server_(rtsp_server),
        id_(id),
        session_name_(std::move(session_name)),
        interface_address_(std::move(interface_address)) {
        RAV_ASSERT(!interface_address_.is_unspecified(), "Address should be specified");
        // Construct a multicast address from the interface address
        const auto interface_address_bytes = interface_address_.to_bytes();
        destination_address_ = asio::ip::address_v4(
            {239, interface_address_bytes[2], interface_address_bytes[3], static_cast<uint8_t>(id.value() % 0xff)}
        );

        // Register handlers for the paths
        by_name_path_ = fmt::format("/by-name/{}", session_name_);
        by_id_path_ = fmt::format("/by-id/{}", id_.to_string());
        auto handler = [this](const rtsp_connection::request_event event) {
            on_request_event(event);
        };
        rtsp_server_.register_handler(by_name_path_, handler);
        rtsp_server_.register_handler(by_id_path_, handler);

        advertisement_id_ = advertiser_.register_service(
            "_rtsp._tcp,_ravenna_session", session_name_.c_str(), nullptr, rtsp_server.port(), {}, false, false
        );
    }

    ~ravenna_transmitter() {
        if (advertisement_id_.is_valid()) {
            advertiser_.unregister_service(advertisement_id_);
        }
        rtsp_server_.register_handler(by_name_path_, nullptr);
        rtsp_server_.register_handler(by_id_path_, nullptr);
    }

    ravenna_transmitter(const ravenna_transmitter& other) = delete;
    ravenna_transmitter& operator=(const ravenna_transmitter& other) = delete;

    ravenna_transmitter(ravenna_transmitter&& other) noexcept = delete;
    ravenna_transmitter& operator=(ravenna_transmitter&& other) noexcept = delete;

    /**
     * @return The transmitter ID.
     */
    [[nodiscard]] id get_id() const {
        return id_;
    }

    /**
     * @return The session name.
     */
    [[nodiscard]] std::string session_name() const {
        return session_name_;
    }

    /**
     * Sets the audio format for the transmitter.
     * @param format The audio format to set.
     * @return True if the audio format is supported, false otherwise.
     */
    bool set_audio_format(const audio_format format) {
        switch (format.encoding) {
            case audio_encoding::undefined:
            case audio_encoding::pcm_s8:
            case audio_encoding::pcm_s32:
            case audio_encoding::pcm_float:
            case audio_encoding::pcm_double:
                return false;
            case audio_encoding::pcm_u8:
                format_.encoding_name = "L8";  // https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.10
            case audio_encoding::pcm_s16:
                format_.encoding_name = "L16";  // https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.11
            case audio_encoding::pcm_s24:
                format_.encoding_name = "L24";  // https://datatracker.ietf.org/doc/html/rfc3190#section-4
                break;
        }
        format_.clock_rate = format.sample_rate;
        format_.num_channels = format.num_channels;
        format_.payload_type = 98;
        return true;
    }

    /**
     * Sets the packet time for 48kHz and 96kHz audio. The packet time will be corrected for other sample rates.
     * @param packet_time The packet time in seconds.
     */
    void set_packet_time(const float packet_time) {
        ptime_ = packet_time;
    }

  private:
    dnssd::dnssd_advertiser& advertiser_;
    rtsp_server& rtsp_server_;
    id id_;
    std::string session_name_;
    asio::ip::address_v4 interface_address_;
    id advertisement_id_;
    std::string by_name_path_;
    std::string by_id_path_;
    int32_t clock_domain_ {};
    sdp::format format_;
    asio::ip::address_v4 destination_address_;
    float ptime_ {1.0f};

    void on_request_event(const rtsp_connection::request_event event) const {
        RAV_TRACE("Received request: {}", event.request.to_debug_string(false));
        const auto sdp = build_sdp();
        const auto encoded = sdp.to_string();
        if (!encoded) {
            RAV_ERROR("Failed to encode SDP");
            return;
        }
        event.connection.async_send_response(rtsp_response(200, "OK", *encoded));
    }

    [[nodiscard]] sdp::session_description build_sdp() const {
        // Connection info
        const sdp::connection_info_field connection_info {
            sdp::netw_type::internet, sdp::addr_type::ipv4, destination_address_.to_string(), 15, {}
        };

        // Source filter
        sdp::source_filter filter(
            sdp::filter_mode::include, sdp::netw_type::internet, sdp::addr_type::ipv4, destination_address_.to_string(),
            {interface_address_.to_string()}
        );

        // Reference clock
        // TODO: Fill in the GMID
        const sdp::reference_clock ref_clock {
            sdp::reference_clock::clock_source::ptp, sdp::reference_clock::ptp_ver::IEEE_1588_2008, "GMID",
            clock_domain_
        };

        // Media clock
        sdp::media_clock_source media_clk {sdp::media_clock_source::clock_mode::direct, 0, {}};

        sdp::ravenna_clock_domain clock_domain {sdp::ravenna_clock_domain::sync_source::ptp_v2, clock_domain_};

        sdp::media_description media;
        media.add_connection_info(connection_info);
        media.set_media_type("audio");
        media.set_port(5004);
        media.set_protocol("RTP/AVP");
        media.add_format(format_);
        media.add_source_filter(filter);
        media.set_clock_domain(clock_domain);
        media.set_sync_time(0);  // TODO: Fill in the sync time
        media.set_ref_clock(ref_clock);
        media.set_direction(sdp::media_direction::recvonly);

        float ptime = ptime_;
        if (auto remainder = 48000 / (format_.clock_rate % 48000); remainder > 0) {
            ptime = static_cast<float>(48000) / static_cast<float>(format_.clock_rate);
        }
        media.set_ptime(ptime);
        media.set_framecount(
            static_cast<uint32_t>(std::round(ptime * static_cast<float>(format_.clock_rate) / 1000.0))
        );

        sdp::session_description sdp;

        // Origin
        const sdp::origin_field o {
            "-", id_.to_string(), 0, sdp::netw_type::internet, sdp::addr_type::ipv4, interface_address_.to_string()
        };
        sdp.set_origin(o);

        // Session name
        sdp.set_session_name(session_name_);
        sdp.set_connection_info(connection_info);
        sdp.set_clock_domain(clock_domain);
        sdp.set_ref_clock(ref_clock);
        sdp.set_media_clock(media_clk);
        sdp.add_media_description(media);

        return sdp;
    }
};

}  // namespace rav
