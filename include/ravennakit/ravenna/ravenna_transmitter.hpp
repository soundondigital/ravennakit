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

#include "ravennakit/aes67/aes67_packet_time.hpp"

#include <utility>

#include "ravennakit/core/uri.hpp"
#include "ravennakit/core/containers/fifo_buffer.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ptp/types/ptp_timestamp.hpp"
#include "ravennakit/rtp/rtp_packet.hpp"
#include "ravennakit/rtp/detail/rtp_transmitter.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

class RavennaTransmitter: public rtsp::server::path_handler {
  public:
    struct OnDataRequestedEvent {
        uint32_t timestamp; // RTP timestamp
        buffer_view<uint8_t> buffer;
    };

    using EventsType = events<OnDataRequestedEvent>;

    RavennaTransmitter(
        asio::io_context& io_context, dnssd::Advertiser& advertiser, rtsp::server& rtsp_server,
        ptp::Instance& ptp_instance, rtp::Transmitter& rtp_transmitter, id id, std::string session_name,
        asio::ip::address_v4 interface_address
    );

    ~RavennaTransmitter() override;

    RavennaTransmitter(const RavennaTransmitter& other) = delete;
    RavennaTransmitter& operator=(const RavennaTransmitter& other) = delete;

    RavennaTransmitter(RavennaTransmitter&& other) noexcept = delete;
    RavennaTransmitter& operator=(RavennaTransmitter&& other) noexcept = delete;

    /**
     * @return The transmitter ID.
     */
    [[nodiscard]] id get_id() const;

    /**
     * @return The session name.
     */
    [[nodiscard]] std::string session_name() const;

    /**
     * Sets the audio format for the transmitter.
     * @param format The audio format to set.
     * @return True if the audio format is supported, false otherwise.
     */
    [[nodiscard]] bool set_audio_format(audio_format format);

    /**
     * Sets the packet time.
     * @param packet_time The packet time.
     */
    void set_packet_time(aes67::PacketTime packet_time);

    /**
     * @return The packet time in milliseconds as signaled using SDP. If the packet time is 1ms and the sample
     * rate is 44.1kHz, then the signaled packet time is 1.09.
     */
    [[nodiscard]] float get_signaled_ptime() const;

    /**
     * Start the streaming.
     * @param timestamp_samples The RTP timestamp in samples at which to send the first packet.
     */
    void start(uint32_t timestamp_samples);

    /**
     * Start the streaming.
     * @param timestamp The timestamp at which to send the first packet.
     */
    void start(ptp::Timestamp timestamp);

    /**
     * Stops the streaming.
     */
    void stop();

    /**
     * @return True if the transmitter is running, false otherwise.
     */
    [[nodiscard]] bool is_running() const;

    /**
     * @return The packet size in number of frames.
     */
    [[nodiscard]] uint32_t get_framecount() const;

    /**
     * Registers a handler for a specific event.
     * @tparam T The event type.
     * @param handler The handler to register.
     */
    template<class T>
    void on(EventsType::handler<T> handler) {
        events_.on(handler);
    }

    // rtsp_server::handler overrides
    void on_request(rtsp::connection::request_event event) const override;

  private:
    dnssd::Advertiser& advertiser_;
    rtsp::server& rtsp_server_;
    ptp::Instance& ptp_instance_;
    rtp::Transmitter& rtp_transmitter_;

    id id_;
    std::string session_name_;
    asio::ip::address_v4 interface_address_;
    asio::ip::address_v4 destination_address_;
    std::string path_by_name_;
    std::string path_by_id_;
    id advertisement_id_;
    int32_t clock_domain_ {};
    audio_format audio_format_;
    sdp::format sdp_format_;  // I think we can compute this from audio_format_ each time we need it
    aes67::PacketTime ptime_ {aes67::PacketTime::ms_1()};
    bool running_ {false};
    ptp::ClockIdentity grandmaster_identity_;
    rtp::Packet rtp_packet_;
    std::vector<uint8_t> packet_intermediate_buffer_;
    asio::high_resolution_timer timer_;
    EventsType events_;
    byte_buffer send_buffer_;
    event_slot<ptp::Instance::ParentChangedEvent> ptp_parent_changed_slot_;

    /**
     * Sends an announce request to all connected clients.
     */
    void send_announce() const;
    [[nodiscard]] sdp::session_description build_sdp() const;
    void start_timer();
    void send_data();
    void resize_internal_buffers();
};

}  // namespace rav
