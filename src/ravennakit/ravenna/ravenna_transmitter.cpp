/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_transmitter.hpp"

#include "ravennakit/rtp/rtp_packet_view.hpp"

#if RAV_WINDOWS
    #include <timeapi.h>
#endif

rav::RavennaTransmitter::RavennaTransmitter(
    asio::io_context& io_context, dnssd::Advertiser& advertiser, rtsp::Server& rtsp_server,
    ptp::Instance& ptp_instance, rtp::Transmitter& rtp_transmitter, const id id, std::string session_name,
    asio::ip::address_v4 interface_address
) :
    advertiser_(advertiser),
    rtsp_server_(rtsp_server),
    ptp_instance_(ptp_instance),
    rtp_transmitter_(rtp_transmitter),
    id_(id),
    session_name_(std::move(session_name)),
    interface_address_(std::move(interface_address)),
    timer_(io_context) {
    RAV_ASSERT(!interface_address_.is_unspecified(), "Address should be specified");

    // Construct a multicast address from the interface address
    const auto interface_address_bytes = interface_address_.to_bytes();
    destination_address_ = asio::ip::address_v4(
        {239, interface_address_bytes[2], interface_address_bytes[3], static_cast<uint8_t>(id.value() % 0xff)}
    );

    // Register handlers for the paths
    path_by_name_ = fmt::format("/by-name/{}", session_name_);
    path_by_id_ = fmt::format("/by-id/{}", id_.to_string());

    rtsp_server_.register_handler(path_by_name_, this);
    rtsp_server_.register_handler(path_by_id_, this);

    advertisement_id_ = advertiser_.register_service(
        "_rtsp._tcp,_ravenna_session", session_name_.c_str(), nullptr, rtsp_server.port(), {}, false, false
    );

    ptp_parent_changed_slot_ =
        ptp_instance.on_parent_changed.subscribe([this](const ptp::Instance::ParentChangedEvent& event) {
            if (grandmaster_identity_ == event.parent.grandmaster_identity) {
                return;
            }
            grandmaster_identity_ = event.parent.grandmaster_identity;
            send_announce();
        });

#if RAV_WINDOWS
    timeBeginPeriod(1);
#endif
}

rav::RavennaTransmitter::~RavennaTransmitter() {
#if RAV_WINDOWS
    timeEndPeriod(1);
#endif

    timer_.cancel();

    if (advertisement_id_.is_valid()) {
        advertiser_.unregister_service(advertisement_id_);
    }

    rtsp_server_.unregister_handler(this);
}

rav::id rav::RavennaTransmitter::get_id() const {
    return id_;
}

std::string rav::RavennaTransmitter::session_name() const {
    return session_name_;
}

bool rav::RavennaTransmitter::set_audio_format(const audio_format format) {
    const auto sdp_format = sdp::Format::from_audio_format(format);
    if (!sdp_format) {
        RAV_ERROR("Failed to convert audio format to SDP format");
        return false;
    }

    if (format == audio_format_) {
        return true;  // Nothing to be done here
    }

    audio_format_ = format;
    sdp_format_ = *sdp_format;

    rtp_packet_.payload_type(sdp_format_.payload_type);
    // TODO: Implement proper SSRC generation
    rtp_packet_.ssrc(static_cast<uint32_t>(random().get_random_int(0, std::numeric_limits<int>::max())));

    return true;
}

void rav::RavennaTransmitter::set_packet_time(const aes67::PacketTime packet_time) {
    if (ptime_ == packet_time) {
        return;
    }
    ptime_ = packet_time;
    resize_internal_buffers();
}

float rav::RavennaTransmitter::get_signaled_ptime() const {
    return ptime_.signaled_ptime(audio_format_.sample_rate);
}

void rav::RavennaTransmitter::start(const uint32_t timestamp_samples) {
    if (running_) {
        return;
    }
    running_ = true;
    rtp_packet_.timestamp(timestamp_samples);
    resize_internal_buffers();
    start_timer();
    RAV_TRACE("Start transmitting at timestamp: {}", timestamp_samples);
}

void rav::RavennaTransmitter::start(const ptp::Timestamp timestamp) {
    start(static_cast<uint32_t>(timestamp.to_samples(audio_format_.sample_rate)));
}

void rav::RavennaTransmitter::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
}

bool rav::RavennaTransmitter::is_running() const {
    return running_;
}

uint32_t rav::RavennaTransmitter::get_framecount() const {
    return ptime_.framecount(audio_format_.sample_rate);
}

void rav::RavennaTransmitter::on_request(rtsp::Connection::RequestEvent event) const {
    const auto sdp = build_sdp();  // Should the SDP be cached and updated on changes?
    RAV_TRACE("SDP:\n{}", sdp.to_string("\n").value());
    const auto encoded = sdp.to_string();
    if (!encoded) {
        RAV_ERROR("Failed to encode SDP");
        return;
    }
    auto response = rtsp::Response(200, "OK", *encoded);
    if (const auto* cseq = event.rtsp_request.rtsp_headers.get("cseq")) {
        response.rtsp_headers.set(*cseq);
    }
    response.rtsp_headers.set("content-type", "application/sdp");
    event.rtsp_connection.async_send_response(response);
}

void rav::RavennaTransmitter::send_announce() const {
    auto sdp = build_sdp().to_string();
    if (!sdp) {
        RAV_ERROR("Failed to encode SDP: {}", sdp.error());
        return;
    }
    rtsp::Request Request;
    request.method = "ANNOUNCE";
    request.rtsp_headers.set("content-type", "application/sdp");
    request.data = std::move(sdp.value());
    request.uri = uri::encode(
        "rtsp://", interface_address_.to_string() + ":" + std::to_string(rtsp_server_.port()), path_by_name_
    );
    rtsp_server_.send_request(path_by_name_, request);
    request.uri =
        uri::encode("rtsp://", interface_address_.to_string() + ":" + std::to_string(rtsp_server_.port()), path_by_id_);
    rtsp_server_.send_request(path_by_name_, request);
}

rav::sdp::SessionDescription rav::RavennaTransmitter::build_sdp() const {
    // Connection info
    const sdp::ConnectionInfoField connection_info {
        sdp::NetwType::internet, sdp::AddrType::ipv4, destination_address_.to_string(), 15, {}
    };

    // Source filter
    sdp::SourceFilter filter(
        sdp::FilterMode::include, sdp::NetwType::internet, sdp::AddrType::ipv4, destination_address_.to_string(),
        {interface_address_.to_string()}
    );

    // Reference clock
    const sdp::ReferenceClock ref_clock {
        sdp::ReferenceClock::ClockSource::ptp, sdp::ReferenceClock::PtpVersion::IEEE_1588_2008,
        grandmaster_identity_.to_string(), clock_domain_
    };

    // Media clock
    // ST 2110-30:2017 defines a constraint to use a zero offset exclusively.
    sdp::MediaClockSource media_clk {sdp::MediaClockSource::ClockMode::direct, 0, {}};

    sdp::RavennaClockDomain clock_domain {sdp::RavennaClockDomain::SyncSource::ptp_v2, clock_domain_};

    sdp::MediaDescription media;
    media.add_connection_info(connection_info);
    media.set_media_type("audio");
    media.set_port(5004);
    media.set_protocol("RTP/AVP");
    media.add_format(sdp_format_);
    media.add_source_filter(filter);
    media.set_clock_domain(clock_domain);
    media.set_sync_time(0);
    media.set_ref_clock(ref_clock);
    media.set_direction(sdp::MediaDirection::recvonly);
    media.set_ptime(get_signaled_ptime());
    media.set_framecount(get_framecount());

    sdp::SessionDescription sdp;

    // Origin
    const sdp::OriginField origin {
        "-", id_.to_string(), 0, sdp::NetwType::internet, sdp::AddrType::ipv4, interface_address_.to_string()
    };
    sdp.set_origin(origin);

    // Session name
    sdp.set_session_name(session_name_);
    sdp.set_connection_info(connection_info);
    sdp.set_clock_domain(clock_domain);
    sdp.set_ref_clock(ref_clock);
    sdp.set_media_clock(media_clk);
    sdp.add_media_description(media);

    return sdp;
}

void rav::RavennaTransmitter::start_timer() {
    TRACY_ZONE_SCOPED;

#if RAV_WINDOWS
    // A dirty hack to get the precision somewhat right. This is only temporary since we have to come up with a much
    // tighter solution.
    auto expires_after = std::chrono::microseconds(1);
#else
    // A tenth of the packet time
    auto expires_after = std::chrono::microseconds(static_cast<int64_t>(get_signaled_ptime() * 1'000 / 10));
#endif

    timer_.expires_after(expires_after);
    timer_.async_wait([this](const asio::error_code ec) {
        if (ec == asio::error::operation_aborted) {
            return;
        }
        if (ec) {
            RAV_ERROR("Timer error: {}", ec.message());
            return;
        }
        send_data();
        start_timer();
    });
}

void rav::RavennaTransmitter::send_data() {
    TRACY_ZONE_SCOPED;

    if (!running_) {
        return;
    }

    const auto framecount = get_framecount();
    const auto required_amount_of_data = framecount * audio_format_.bytes_per_frame();
    RAV_ASSERT(packet_intermediate_buffer_.size() == required_amount_of_data, "Buffer size mismatch");

    for (auto i = 0; i < 1000; i++) {
        const auto now_samples = ptp_instance_.get_local_ptp_time().to_samples(audio_format_.sample_rate);
        if (wrapping_uint(static_cast<uint32_t>(now_samples)) < rtp_packet_.timestamp()) {
            break;
        }

        events_.emit(
            OnDataRequestedEvent {rtp_packet_.timestamp().value(), buffer_view(packet_intermediate_buffer_)}
        );

        if (audio_format_.byte_order == audio_format::byte_order::le) {
            swap_bytes(packet_intermediate_buffer_.data(), required_amount_of_data, audio_format_.bytes_per_sample());
        }

        send_buffer_.clear();
        rtp_packet_.encode(packet_intermediate_buffer_.data(), required_amount_of_data, send_buffer_);
        rtp_packet_.sequence_number_inc(1);
        rtp_packet_.timestamp_inc(framecount);
        rtp_transmitter_.send_to(send_buffer_, {destination_address_, 5004});
    }
}

void rav::RavennaTransmitter::resize_internal_buffers() {
    const auto bytes_per_packet = get_framecount() * audio_format_.bytes_per_frame();
    packet_intermediate_buffer_.resize(bytes_per_packet);
}
