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

#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/core/audio/audio_data.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

#if RAV_WINDOWS
    #include <timeapi.h>
#endif

rav::RavennaSender::RavennaSender(
    asio::io_context& io_context, dnssd::Advertiser& advertiser, rtsp::Server& rtsp_server, ptp::Instance& ptp_instance,
    const Id id, const asio::ip::address_v4& interface_address, ConfigurationUpdate initial_config
) :
    StreamSender(io_context, interface_address),
    advertiser_(advertiser),
    rtsp_server_(rtsp_server),
    ptp_instance_(ptp_instance),
    id_(id),
    timer_(io_context) {
    if (!initial_config.session_name.has_value()) {
        initial_config.session_name = "Sender " + id_.to_string();
    }

    if (!initial_config.ttl.has_value()) {
        initial_config.ttl = 15;
    }

    if (!initial_config.packet_time.has_value()) {
        initial_config.packet_time = aes67::PacketTime::ms_1();
    }

    if (!initial_config.payload_type.has_value()) {
        initial_config.payload_type = 98;
    }

    if (!initial_config.audio_format.has_value()) {
        AudioFormat audio_format;
        audio_format.byte_order = AudioFormat::ByteOrder::be;
        audio_format.ordering = AudioFormat::ChannelOrdering::interleaved;
        audio_format.sample_rate = 48'000;
        audio_format.num_channels = 2;
        audio_format.encoding = AudioEncoding::pcm_s16;
        initial_config.audio_format = audio_format;
    }

    if (!initial_config.destination_address.has_value()) {
        // Construct a multicast address from the interface address
        const auto interface_address_bytes = interface_address.to_bytes();
        configuration_.destination_address = asio::ip::address_v4(
            {239, interface_address_bytes[2], interface_address_bytes[3], static_cast<uint8_t>(id_.value() % 0xff)}
        );
    }

    if (!ptp_instance_.subscribe(this)) {
        RAV_ERROR("Failed to subscribe to PTP instance");
    }

#if RAV_WINDOWS
    timeBeginPeriod(1);
#endif

    update_configuration(initial_config);
}

rav::RavennaSender::~RavennaSender() {
    if (!ptp_instance_.unsubscribe(this)) {
        RAV_ERROR("Failed to unsubscribe from PTP instance");
    }

#if RAV_WINDOWS
    timeEndPeriod(1);
#endif

    if (advertisement_id_.is_valid()) {
        advertiser_.unregister_service(advertisement_id_);
    }

    rtsp_server_.unregister_handler(this);
}

rav::Id rav::RavennaSender::get_id() const {
    return id_;
}

tl::expected<void, std::string> rav::RavennaSender::update_configuration(const ConfigurationUpdate& update) {
    std::ignore = shared_context_.reclaim();  // TODO: Do somewhere else, maybe on a timer or something.

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
    bool announce = false;

    // Session name
    if (update.session_name.has_value() && update.session_name != configuration_.session_name) {
        configuration_.session_name = *update.session_name;
        update_advertisement = true;
        announce = true;
    }

    // Destination address
    if (update.destination_address.has_value() && update.destination_address != configuration_.destination_address) {
        configuration_.destination_address = *update.destination_address;
        announce = true;
    }

    // TTL
    if (update.ttl.has_value() && update.ttl != configuration_.ttl) {
        configuration_.ttl = *update.ttl;
        announce = true;
        // TODO: Update socket option for TTL. Probably both multicast and unicast in one go.
    }

    // Payload type
    if (update.payload_type.has_value() && update.payload_type != configuration_.payload_type) {
        configuration_.payload_type = *update.payload_type;
        announce = true;
    }

    // Audio format
    if (update.audio_format.has_value() && update.audio_format != configuration_.audio_format) {
        configuration_.audio_format = *update.audio_format;
        announce = true;
    }

    // Packet time
    if (update.packet_time.has_value() && update.packet_time != configuration_.packet_time) {
        configuration_.packet_time = *update.packet_time;
        announce = true;
    }

    // Enabled
    if (update.enabled.has_value() && update.enabled != configuration_.enabled) {
        configuration_.enabled = *update.enabled;
        configuration_.enabled ? start_timer() : stop_timer();
    }

    // Adjust timestamps
    if (update.adjust_timestamps.has_value() && update.adjust_timestamps != configuration_.adjust_timestamps) {
        configuration_.adjust_timestamps = *update.adjust_timestamps;
    }

    const bool should_be_running = configuration_.enabled && !configuration_.session_name.empty()
        && configuration_.audio_format.is_valid() && configuration_.destination_address.is_multicast()
        && configuration_.packet_time.is_valid() && configuration_.ttl > 0;

    if (update_advertisement || !should_be_running) {
        RAV_ASSERT(
            rtsp_path_by_id_.empty() == rtsp_path_by_name_.empty(), "Paths should be either both empty or both set"
        );

        if (!rtsp_path_by_id_.empty()) {
            RAV_TRACE("Unregistering RTSP path handler");
            rtsp_server_.unregister_handler(this);
            rtsp_path_by_name_.clear();
            rtsp_path_by_id_.clear();
        }

        // Stop DNS-SD advertisement
        if (advertisement_id_.is_valid()) {
            RAV_TRACE("Unregistering sender advertisement");
            advertiser_.unregister_service(advertisement_id_);
            advertisement_id_ = {};
        }
    }

    for (const auto& subscriber : subscribers_) {
        subscriber->ravenna_sender_configuration_updated(id_, configuration_);
    }

    if (!should_be_running) {
        shared_context_.clear();
        return {};  // Done here
    }

    if (rtsp_path_by_id_.empty()) {
        RAV_ASSERT(
            rtsp_path_by_id_.empty() == rtsp_path_by_name_.empty(), "Paths should be either both empty or both set"
        );

        // Register handlers for the paths
        rtsp_path_by_name_ = fmt::format("/by-name/{}", configuration_.session_name);
        rtsp_path_by_id_ = fmt::format("/by-id/{}", id_.to_string());

        RAV_TRACE("Registering RTSP path handler for paths {} and {}", rtsp_path_by_name_, rtsp_path_by_id_);

        rtsp_server_.register_handler(rtsp_path_by_name_, this);
        rtsp_server_.register_handler(rtsp_path_by_id_, this);
    }

    if (!advertisement_id_.is_valid()) {
        RAV_TRACE("Registering sender advertisement");
        advertisement_id_ = advertiser_.register_service(
            "_rtsp._tcp,_ravenna_session", configuration_.session_name.c_str(), nullptr, rtsp_server_.port(), {}, false,
            false
        );
    }

    if (announce) {
        send_announce();
    }

    update_realtime_context();

    return {};
}

const rav::RavennaSender::Configuration& rav::RavennaSender::get_configuration() const {
    return configuration_;
}

float rav::RavennaSender::get_signaled_ptime() const {
    return configuration_.packet_time.signaled_ptime(configuration_.audio_format.sample_rate);
}

bool rav::RavennaSender::subscribe(Subscriber* subscriber) {
    if (subscribers_.add(subscriber)) {
        subscriber->ravenna_sender_configuration_updated(id_, configuration_);
        return true;
    }
    return false;
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

bool rav::RavennaSender::send_data_realtime(const BufferView<const uint8_t> buffer, const uint32_t timestamp) {
    if (!ptp_stable_) {
        return false;
    }

    if (auto lock = send_data_realtime_reader_.lock_realtime()) {
        auto& rtp_buffer = lock->rtp_buffer;
        auto& rtp_packet = lock->rtp_packet;
        const auto packet_time_frames = lock->packet_time_frames;
        const auto size_per_packet = packet_time_frames * lock->audio_format.bytes_per_frame();

        if (rtp_buffer.get_next_ts() != WrappingUint32(timestamp)) {
            // This buffer is not at the expected timestamp, reset the timestamp
            rtp_packet.set_timestamp(timestamp);
            rtp_buffer.set_next_ts(timestamp);
            // TODO: Should the sequence number be reset as well?
        }

        rtp_buffer.clear_until(timestamp);
        rtp_buffer.write(timestamp, buffer);

        const auto next_ts = rtp_buffer.get_next_ts();

        while (rtp_packet.get_timestamp() + packet_time_frames < next_ts) {
            TRACY_ZONE_SCOPED;

            rtp_buffer.read(rtp_packet.get_timestamp().value(), lock->intermediate_send_buffer.data(), size_per_packet);

            lock->rtp_packet_buffer.clear();
            rtp_packet.encode(lock->intermediate_send_buffer.data(), size_per_packet, lock->rtp_packet_buffer);

            if (lock->rtp_packet_buffer.size() > aes67::constants::k_max_payload) {
                RAV_ERROR("Exceeding max payload: {}", lock->rtp_packet_buffer.size());
                return false;
            }

            Packet packet;
            packet.rtp_timestamp = rtp_packet.get_timestamp().value();
            packet.payload_size_bytes = static_cast<uint32_t>(lock->rtp_packet_buffer.size());
            std::memcpy(packet.payload.data(), lock->rtp_packet_buffer.data(), lock->rtp_packet_buffer.size());

            lock->outgoing_data.push(packet);

            rtp_packet.sequence_number_inc(1);
            rtp_packet.inc_timestamp(packet_time_frames);
        }

        return true;
    }

    return false;
}

bool rav::RavennaSender::send_audio_data_realtime(
    const AudioBufferView<const float>& input_buffer, const uint32_t timestamp
) {
    TRACY_ZONE_SCOPED;

    if (input_buffer.find_max_abs() > 1.0f) {
        RAV_WARNING("Audio overload");
    }

    if (input_buffer.num_frames() > k_max_num_frames) {
        RAV_WARNING("Input buffer size exceeds maximum size");
        return false;
    }

    if (auto lock = send_data_realtime_reader_.lock_realtime()) {
        auto audio_format = lock->audio_format;
        if (audio_format.num_channels != input_buffer.num_channels()) {
            RAV_ERROR("Channel mismatch: expected {}, got {}", audio_format.num_channels, input_buffer.num_channels());
            return false;
        }

        auto& intermediate_buffer = lock->intermediate_audio_buffer;

        if (audio_format.encoding == AudioEncoding::pcm_s16) {
            const auto ok = AudioData::convert<
                float, AudioData::ByteOrder::Ne, int16_t, AudioData::ByteOrder::Be,
                AudioData::Interleaving::Interleaved>(
                input_buffer.data(), input_buffer.num_frames(), input_buffer.num_channels(),
                reinterpret_cast<int16_t*>(intermediate_buffer.data()), 0, 0
            );
            if (!ok) {
                RAV_WARNING("Failed to convert audio data");
            }
        } else if (audio_format.encoding == AudioEncoding::pcm_s24) {
            const auto ok = AudioData::convert<
                float, AudioData::ByteOrder::Ne, int24_t, AudioData::ByteOrder::Be,
                AudioData::Interleaving::Interleaved>(
                input_buffer.data(), input_buffer.num_frames(), input_buffer.num_channels(),
                reinterpret_cast<int24_t*>(intermediate_buffer.data()), 0, 0
            );
            if (!ok) {
                RAV_WARNING("Failed to convert audio data");
            }
        } else {
            RAV_ERROR("Unsupported encoding");
            return false;
        }

        return send_data_realtime(
            BufferView(intermediate_buffer)
                .subview(0, input_buffer.num_frames() * audio_format.bytes_per_frame())
                .const_view(),
            timestamp
        );
    }

    return false;
}

void rav::RavennaSender::on_data_requested(OnDataRequestedHandler handler) {
    on_data_requested_handler_ = std::move(handler);
}

void rav::RavennaSender::on_request(const rtsp::Connection::RequestEvent event) const {
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

void rav::RavennaSender::ptp_parent_changed(const ptp::ParentDs& parent) {
    if (grandmaster_identity_ == parent.grandmaster_identity) {
        return;
    }
    grandmaster_identity_ = parent.grandmaster_identity;
    send_announce();
}

void rav::RavennaSender::ptp_port_changed_state(const ptp::Port& port) {
    ptp_stable_ = port.state() == ptp::State::slave || port.state() == ptp::State::master;
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
    request.uri =
        Uri::encode("rtsp", interface_address_string + ":" + std::to_string(rtsp_server_.port()), rtsp_path_by_name_);
    rtsp_server_.send_request(rtsp_path_by_name_, request);
    RAV_TRACE("Announced session: {}", request.uri);

    request.uri =
        Uri::encode("rtsp", interface_address_string + ":" + std::to_string(rtsp_server_.port()), rtsp_path_by_id_);
    rtsp_server_.send_request(rtsp_path_by_name_, request);
    RAV_TRACE("Announced session: {}", request.uri);
}

rav::sdp::SessionDescription rav::RavennaSender::build_sdp() const {
    if (configuration_.destination_address.is_unspecified()) {
        RAV_ERROR("Destination address not set");
        return {};
    }

    if (configuration_.session_name.empty()) {
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
    // tighter solution anyway.
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
        std::lock_guard lock(mutex_);
        send_outgoing_data();
        start_timer();
    });
}

void rav::RavennaSender::stop_timer() {
    std::lock_guard lock(mutex_);
    timer_.cancel();
}

void rav::RavennaSender::send_outgoing_data() {
    if (auto lock = send_outgoing_data_reader_.lock_realtime()) {
        TRACY_ZONE_SCOPED;

        const auto packet = lock->outgoing_data.pop();

        if (!packet.has_value()) {
            return;  // Nothing to do here
        }

        const auto ptp_ts =
            static_cast<uint32_t>(ptp_instance_.get_local_ptp_time().to_samples(lock->audio_format.sample_rate));

        TRACY_PLOT("ts diff", static_cast<int64_t>(WrappingUint32(ptp_ts).diff(WrappingUint32(packet->rtp_timestamp))));

        RAV_ASSERT(packet->payload_size_bytes <= aes67::constants::k_max_payload, "Payload size exceeds maximum");

        rtp_sender_.send_to(packet->payload.data(), packet->payload_size_bytes, lock->destination_endpoint);
    }
}

void rav::RavennaSender::update_realtime_context() {
    // TODO: Implement proper SSRC generation
    const auto ssrc = static_cast<uint32_t>(Random().get_random_int(0, std::numeric_limits<int>::max()));

    auto new_context = std::make_unique<SharedContext>();
    const auto audio_format = configuration_.audio_format;
    const auto packet_size_frames = get_framecount();
    const auto packet_size_bytes = packet_size_frames * audio_format.bytes_per_frame();
    new_context->destination_endpoint = {configuration_.destination_address, 5004};
    new_context->audio_format = audio_format;
    new_context->packet_time_frames = get_framecount();
    new_context->rtp_packet.payload_type(configuration_.payload_type);
    new_context->rtp_packet.ssrc(ssrc);
    new_context->outgoing_data.resize(k_buffer_num_packets);
    new_context->intermediate_audio_buffer.resize(k_max_num_frames * audio_format.bytes_per_frame());
    new_context->rtp_packet_buffer = ByteBuffer(packet_size_bytes);
    new_context->intermediate_send_buffer.resize(packet_size_bytes);
    new_context->adjust_timestamps = configuration_.adjust_timestamps;
    new_context->rtp_buffer.resize(k_max_num_frames, audio_format.bytes_per_frame());
    new_context->rtp_buffer.set_ground_value(audio_format.ground_value());

    shared_context_.update(std::move(new_context));
}
