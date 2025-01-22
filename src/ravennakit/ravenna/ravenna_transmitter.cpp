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

rav::ravenna_transmitter::ravenna_transmitter(
    asio::io_context& io_context, dnssd::dnssd_advertiser& advertiser, rtsp_server& rtsp_server,
    ptp_instance& ptp_instance, rtp_transmitter& rtp_transmitter, const id id, std::string session_name,
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
    auto handler = [this](const rtsp_connection::request_event event) {
        on_request_event(event);
    };
    rtsp_server_.register_handler(path_by_name_, handler);
    rtsp_server_.register_handler(path_by_id_, handler);

    advertisement_id_ = advertiser_.register_service(
        "_rtsp._tcp,_ravenna_session", session_name_.c_str(), nullptr, rtsp_server.port(), {}, false, false
    );

    ptp_instance_.add_subscriber(this);
}

rav::ravenna_transmitter::~ravenna_transmitter() {
    timer_.cancel();

    ptp_instance_.remove_subscriber(this);

    if (advertisement_id_.is_valid()) {
        advertiser_.unregister_service(advertisement_id_);
    }

    rtsp_server_.register_handler(path_by_name_, nullptr);
    rtsp_server_.register_handler(path_by_id_, nullptr);
}

rav::id rav::ravenna_transmitter::get_id() const {
    return id_;
}

std::string rav::ravenna_transmitter::session_name() const {
    return session_name_;
}

bool rav::ravenna_transmitter::set_audio_format(const audio_format format) {
    const auto sdp_format = sdp::format::from_audio_format(format);
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
    rtp_packet_.ssrc(
        random().get_random_int(0, std::numeric_limits<int>::max())  // TODO: Implement SSRC generation
    );

    return true;
}

void rav::ravenna_transmitter::set_packet_time(const aes67_packet_time packet_time) {
    if (ptime_ == packet_time) {
        return;
    }
    ptime_ = packet_time;
    resize_internal_buffers();
}

float rav::ravenna_transmitter::get_signaled_ptime() const {
    return ptime_.signaled_ptime(audio_format_.sample_rate);
}

void rav::ravenna_transmitter::start(const ptp_timestamp timestamp) {
    if (running_) {
        return;
    }
    running_ = true;
    rtp_packet_.timestamp(static_cast<uint32_t>(timestamp.to_samples(audio_format_.sample_rate)));
    resize_internal_buffers();
    start_timer();
}

void rav::ravenna_transmitter::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
}

bool rav::ravenna_transmitter::is_running() const {
    return running_;
}

void rav::ravenna_transmitter::feed_audio_date(const uint8_t* data, const size_t size) {
    RAV_ASSERT(data != nullptr, "Data is null");
    RAV_ASSERT(size > 0, "Size is 0");
    fifo_buffer_.write(data, size);
}

uint32_t rav::ravenna_transmitter::get_framecount() const {
    return ptime_.framecount(audio_format_.sample_rate);
}

void rav::ravenna_transmitter::on_parent_changed(const ptp_parent_ds& parent) {
    if (grandmaster_identity_ == parent.grandmaster_identity) {
        return;
    }
    grandmaster_identity_ = parent.grandmaster_identity;
    send_announce();
}

void rav::ravenna_transmitter::on_request_event(const rtsp_connection::request_event event) const {
    const auto sdp = build_sdp();  // Should the SDP be cached and updated on changes?
    RAV_TRACE("SDP:\n{}", sdp.to_string("\n").value());
    const auto encoded = sdp.to_string();
    if (!encoded) {
        RAV_ERROR("Failed to encode SDP");
        return;
    }
    const auto response = rtsp_response(200, "OK", *encoded);
    response.headers["content-type"] = "application/sdp";
    event.connection.async_send_response(response);
}

void rav::ravenna_transmitter::send_announce() const {
    auto sdp = build_sdp().to_string();
    if (!sdp) {
        RAV_ERROR("Failed to encode SDP: {}", sdp.error());
        return;
    }
    rtsp_request request;
    request.method = "ANNOUNCE";
    request.headers["content-type"] = "application/sdp";
    request.data = std::move(sdp.value());
    request.uri = uri::encode(
        "rtsp://", interface_address_.to_string() + ":" + std::to_string(rtsp_server_.port()), path_by_name_
    );
    rtsp_server_.send_request(path_by_name_, request);
    request.uri =
        uri::encode("rtsp://", interface_address_.to_string() + ":" + std::to_string(rtsp_server_.port()), path_by_id_);
    rtsp_server_.send_request(path_by_name_, request);
}

rav::sdp::session_description rav::ravenna_transmitter::build_sdp() const {
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
    const sdp::reference_clock ref_clock {
        sdp::reference_clock::clock_source::ptp, sdp::reference_clock::ptp_ver::IEEE_1588_2008,
        grandmaster_identity_.to_string(), clock_domain_
    };

    // Media clock
    // ST 2110-30:2017 defines a constraint to use a zero offset exclusively.
    sdp::media_clock_source media_clk {sdp::media_clock_source::clock_mode::direct, 0, {}};

    sdp::ravenna_clock_domain clock_domain {sdp::ravenna_clock_domain::sync_source::ptp_v2, clock_domain_};

    sdp::media_description media;
    media.add_connection_info(connection_info);
    media.set_media_type("audio");
    media.set_port(5004);
    media.set_protocol("RTP/AVP");
    media.add_format(sdp_format_);
    media.add_source_filter(filter);
    media.set_clock_domain(clock_domain);
    media.set_sync_time(0);
    media.set_ref_clock(ref_clock);
    media.set_direction(sdp::media_direction::recvonly);
    media.set_ptime(get_signaled_ptime());
    media.set_framecount(get_framecount());

    sdp::session_description sdp;

    // Origin
    const sdp::origin_field origin {
        "-", id_.to_string(), 0, sdp::netw_type::internet, sdp::addr_type::ipv4, interface_address_.to_string()
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

void rav::ravenna_transmitter::start_timer() {
    TRACY_ZONE_SCOPED;

    timer_.expires_after(
        std::chrono::microseconds(
            static_cast<int64_t>(get_signaled_ptime() * 1'000 / 10)
        )  // A tenth of the packet time
    );
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

void rav::ravenna_transmitter::send_data() {
    TRACY_ZONE_SCOPED;

    if (!running_) {
        return;
    }

    const auto now_samples = ptp_instance_.get_local_ptp_time().to_samples(audio_format_.sample_rate);
    const auto framecount = get_framecount();
    if (sequence_number(static_cast<uint32_t>(now_samples)) < rtp_packet_.timestamp()) {
        return;  // Not time to send data yet
    }

    const auto required_amount_of_data = framecount * audio_format_.bytes_per_frame();

    if (fifo_buffer_.size() < required_amount_of_data) {
        // TODO: Callback asking for data
        return;
    }

    RAV_ASSERT(packet_intermediate_buffer_.size() == required_amount_of_data, "Buffer size mismatch");
    fifo_buffer_.read(packet_intermediate_buffer_.data(), required_amount_of_data);

    byte_buffer buffer;
    rtp_packet_.encode(packet_intermediate_buffer_.data(), required_amount_of_data, buffer);
    rtp_packet_.sequence_number_inc(1);
    rtp_packet_.timestamp_inc(framecount);

    rtp_transmitter_.send_to(buffer, {destination_address_, 5004});
}

void rav::ravenna_transmitter::resize_internal_buffers() {
    const auto bytes_per_packet = get_framecount() * audio_format_.bytes_per_frame();
    fifo_buffer_.resize(bytes_per_packet * k_fifo_buffer_multiplier_packet_times);
    packet_intermediate_buffer_.resize(bytes_per_packet);
}
