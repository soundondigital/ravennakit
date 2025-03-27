/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_sender.hpp"

#include "ravennakit/rtp/rtp_packet_view.hpp"

#if RAV_WINDOWS
    #include <timeapi.h>
#endif

rav::RavennaSender::RavennaSender(
    asio::io_context& io_context, dnssd::Advertiser& advertiser, rtsp::Server& rtsp_server, ptp::Instance& ptp_instance,
    rtp::Sender& rtp_sender, const Id id
) :
    advertiser_(advertiser),
    rtsp_server_(rtsp_server),
    ptp_instance_(ptp_instance),
    rtp_sender_(rtp_sender),
    id_(id),
    timer_(io_context) {
    // Set defaults
    configuration_.session_name = "Sender " + id_.to_string();
    configuration_.ttl = 15;
    configuration_.packet_time = aes67::PacketTime::ms_1();
    configuration_.payload_type = 98;
    configuration_.audio_format.byte_order = AudioFormat::ByteOrder::be;
    configuration_.audio_format.ordering = AudioFormat::ChannelOrdering::interleaved;
    configuration_.audio_format.sample_rate = 48'000;
    configuration_.audio_format.num_channels = 2;
    configuration_.enabled = false;

    // Construct a multicast address from the interface address
    const auto interface_address_bytes = rtp_sender_.get_interface_address().to_bytes();
    configuration_.destination_address = asio::ip::address_v4(
        {239, interface_address_bytes[2], interface_address_bytes[3], static_cast<uint8_t>(id_.value() % 0xff)}
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

rav::RavennaSender::~RavennaSender() {
#if RAV_WINDOWS
    timeEndPeriod(1);
#endif

    timer_.cancel();

    if (advertisement_id_.is_valid()) {
        advertiser_.unregister_service(advertisement_id_);
    }

    rtsp_server_.unregister_handler(this);
}

rav::Id rav::RavennaSender::get_id() const {
    return id_;
}

tl::expected<void, std::string> rav::RavennaSender::update_configuration(const ConfigurationUpdate& update) {
    // Session name
    if (update.session_name.has_value()) {
        if (update.session_name->empty()) {
            return tl::unexpected("Session name cannot be empty");
        }
    }

    // Destination address
    if (update.destination_address.has_value()) {
        if (update.destination_address->is_unspecified()) {
            return tl::unexpected("Destination address cannot be unspecified");
        }
        if (!update.destination_address->is_multicast()) {
            return tl::unexpected("Destination address must be multicast");
        }
    }

    // TTL
    if (update.ttl.has_value()) {
        if (update.ttl == 0) {
            return tl::unexpected("TTL cannot be 0");
        }
    }

    // Audio format
    if (update.audio_format.has_value()) {
        if (!update.audio_format->is_valid()) {
            return tl::unexpected("Invalid audio format");
        }
        if (update.audio_format->ordering != AudioFormat::ChannelOrdering::interleaved) {
            return tl::unexpected("Only interleaved audio formats are supported");
        }
        if (update.audio_format->byte_order != AudioFormat::ByteOrder::be) {
            return tl::unexpected("Only big endian audio formats are supported");
        }
    }

    // Packet time
    if (update.packet_time.has_value()) {
        if (!update.packet_time->is_valid()) {
            return tl::unexpected("Invalid packet time");
        }
    }

    bool update_advertisement = false;
    bool schedule_announce = false;

    // Session name
    if (update.session_name.has_value() && update.session_name != configuration_.session_name) {
        configuration_.session_name = *update.session_name;
        update_advertisement = true;
        schedule_announce = true;
    }

    // Destination address
    if (update.destination_address.has_value() && update.destination_address != configuration_.destination_address) {
        configuration_.destination_address = *update.destination_address;
        schedule_announce = true;
    }

    // TTL
    if (update.ttl.has_value() && update.ttl != configuration_.ttl) {
        configuration_.ttl = *update.ttl;
        schedule_announce = true;
        // TODO: Update socket option for TTL. Probably both multicast and unicast in one go.
    }

    // Payload type
    if (update.payload_type.has_value() && update.payload_type != configuration_.payload_type) {
        configuration_.payload_type = *update.payload_type;
        schedule_announce = true;
    }

    // Audio format
    if (update.audio_format.has_value() && update.audio_format != configuration_.audio_format) {
        configuration_.audio_format = *update.audio_format;
        schedule_announce = true;
    }

    // Packet time
    if (update.packet_time.has_value() && update.packet_time != configuration_.packet_time) {
        configuration_.packet_time = *update.packet_time;
        schedule_announce = true;
    }

    RAV_ASSERT(!configuration_.session_name.empty(), "Session name should not be empty");
    RAV_ASSERT(configuration_.audio_format.is_valid(), "Audio format should be valid");
    RAV_ASSERT(configuration_.destination_address.is_multicast(), "Destination address should be multicast");
    RAV_ASSERT(configuration_.packet_time.is_valid(), "Packet time should be valid");
    RAV_ASSERT(configuration_.ttl > 0, "TTL should be greater than 0");

    if (update_advertisement || !configuration_.enabled) {
        RAV_ASSERT(
            rtsp_path_by_id_.empty() == rtsp_path_by_name_.empty(), "Paths should be either both empty or both set"
        );
        rtsp_server_.unregister_handler(this);
        rtsp_path_by_name_.clear();
        rtsp_path_by_id_.clear();

        // Stop DNS-SD advertisement
        if (advertisement_id_.is_valid()) {
            advertiser_.unregister_service(advertisement_id_);
            advertisement_id_ = {};
        }
    }

    if (configuration_.enabled == false) {
        return {};  // Done here
    }

    if (rtsp_path_by_id_.empty()) {
        RAV_ASSERT(
            rtsp_path_by_id_.empty() == rtsp_path_by_name_.empty(), "Paths should be either both empty or both set"
        );

        // Register handlers for the paths
        rtsp_path_by_name_ = fmt::format("/by-name/{}", configuration_.session_name);
        rtsp_path_by_id_ = fmt::format("/by-id/{}", id_.to_string());

        rtsp_server_.register_handler(rtsp_path_by_name_, this);
        rtsp_server_.register_handler(rtsp_path_by_id_, this);
    }

    if (!advertisement_id_.is_valid()) {
        advertisement_id_ = advertiser_.register_service(
            "_rtsp._tcp,_ravenna_session", configuration_.session_name.c_str(), nullptr, rtsp_server_.port(), {}, false,
            false
        );
    }

    rtp_packet_.payload_type(configuration_.payload_type);
    // TODO: Implement proper SSRC generation
    rtp_packet_.ssrc(static_cast<uint32_t>(Random().get_random_int(0, std::numeric_limits<int>::max())));

    if (schedule_announce) {
        send_announce();
    }

    resize_internal_buffers();  // TODO: Remove

    return {};
}

const rav::RavennaSender::Configuration& rav::RavennaSender::get_configuration() const {
    return configuration_;
}

float rav::RavennaSender::get_signaled_ptime() const {
    return configuration_.packet_time.signaled_ptime(configuration_.audio_format.sample_rate);
}

bool rav::RavennaSender::subscribe(Subscriber* subscriber) {
    return subscribers_.add(subscriber);
}

bool rav::RavennaSender::unsubscribe(Subscriber* subscriber) {
    if (subscribers_.remove(subscriber)) {
        subscriber->ravenna_sender_configuration_updated(id_, configuration_);
        return true;
    }
    return false;
}

uint32_t rav::RavennaSender::get_framecount() const {
    return configuration_.packet_time.framecount(configuration_.audio_format.sample_rate);
}

void rav::RavennaSender::on_data_requested(OnDataRequestedHandler handler) {
    on_data_requested_handler_ = std::move(handler);
}

void rav::RavennaSender::on_request(rtsp::Connection::RequestEvent event) const {
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

void rav::RavennaSender::send_announce() const {
    auto sdp = build_sdp().to_string();
    if (!sdp) {
        RAV_ERROR("Failed to encode SDP: {}", sdp.error());
        return;
    }

    auto interface_address_string = rtp_sender_.get_interface_address().to_string();

    rtsp::Request request;
    request.method = "ANNOUNCE";
    request.rtsp_headers.set("content-type", "application/sdp");
    request.data = std::move(sdp.value());
    request.uri = Uri::encode(
        "rtsp://", interface_address_string + ":" + std::to_string(rtsp_server_.port()), rtsp_path_by_name_
    );
    rtsp_server_.send_request(rtsp_path_by_name_, request);
    request.uri =
        Uri::encode("rtsp://", interface_address_string + ":" + std::to_string(rtsp_server_.port()), rtsp_path_by_id_);
    rtsp_server_.send_request(rtsp_path_by_name_, request);
}

rav::sdp::SessionDescription rav::RavennaSender::build_sdp() const {
    if (configuration_.destination_address.is_unspecified()) {
        RAV_ERROR("Destination address not set");
        return {};
    }

    if (!configuration_.session_name.empty()) {
        RAV_ERROR("Session name not set");
        return {};
    }

    const auto sdp_format = sdp::Format::from_audio_format(configuration_.audio_format);
    if (!sdp_format) {
        RAV_ERROR("Failed to convert audio format to SDP format");
        return {};
    }

    // Connection info
    const sdp::ConnectionInfoField connection_info {
        sdp::NetwType::internet, sdp::AddrType::ipv4, configuration_.destination_address.to_string(), 15, {}
    };

    // Source filter
    sdp::SourceFilter filter(
        sdp::FilterMode::include, sdp::NetwType::internet, sdp::AddrType::ipv4,
        configuration_.destination_address.to_string(), {rtp_sender_.get_interface_address().to_string()}
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
    media.add_format(sdp_format.value());
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
        "-",
        id_.to_string(),
        0,
        sdp::NetwType::internet,
        sdp::AddrType::ipv4,
        rtp_sender_.get_interface_address().to_string(),
    };
    sdp.set_origin(origin);

    // Session name
    sdp.set_session_name(configuration_.session_name);
    sdp.set_connection_info(connection_info);
    sdp.set_clock_domain(clock_domain);
    sdp.set_ref_clock(ref_clock);
    sdp.set_media_clock(media_clk);
    sdp.add_media_description(media);

    return sdp;
}

void rav::RavennaSender::start_timer() {
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

void rav::RavennaSender::send_data() {
    TRACY_ZONE_SCOPED;

    if (!on_data_requested_handler_) {
        RAV_WARNING("No data handler installed");
        return;
    }

    if (configuration_.destination_address.is_unspecified()) {
        RAV_ERROR("Destination address not set");
        return;
    }

    const auto framecount = get_framecount();
    const auto required_amount_of_data = framecount * configuration_.audio_format.bytes_per_frame();
    RAV_ASSERT(packet_intermediate_buffer_.size() == required_amount_of_data, "Buffer size mismatch");

    for (auto i = 0; i < 10; i++) {
        const auto now_samples = ptp_instance_.get_local_ptp_time().to_samples(configuration_.audio_format.sample_rate);
        if (WrappingUint(static_cast<uint32_t>(now_samples)) < rtp_packet_.timestamp()) {
            break;
        }

        if (!on_data_requested_handler_(rtp_packet_.timestamp().value(), BufferView(packet_intermediate_buffer_))) {
            return;  // No data was provided
        }

        if (configuration_.audio_format.byte_order == AudioFormat::ByteOrder::le) {
            swap_bytes(
                packet_intermediate_buffer_.data(), required_amount_of_data,
                configuration_.audio_format.bytes_per_sample()
            );
        }

        send_buffer_.clear();
        rtp_packet_.encode(packet_intermediate_buffer_.data(), required_amount_of_data, send_buffer_);
        rtp_packet_.sequence_number_inc(1);
        rtp_packet_.timestamp_inc(framecount);
        rtp_sender_.send_to(send_buffer_, {configuration_.destination_address, 5004});
    }
}

void rav::RavennaSender::resize_internal_buffers() {
    const auto bytes_per_packet = get_framecount() * configuration_.audio_format.bytes_per_frame();
    packet_intermediate_buffer_.resize(bytes_per_packet);
}
