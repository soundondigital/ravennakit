/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_receiver.hpp"

#include "ravennakit/aes67/aes67_packet_time.hpp"
#include "ravennakit/core/audio/audio_data.hpp"
#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/rtp/detail/rtp_filter.hpp"

namespace {

bool is_connection_info_valid(const rav::sdp::ConnectionInfoField& conn) {
    if (conn.network_type != rav::sdp::NetwType::internet) {
        RAV_WARNING("Unsupported network type in connection_info_field");
        return false;
    }
    if (!(conn.address_type == rav::sdp::AddrType::ipv4 || conn.address_type == rav::sdp::AddrType::ipv6)) {
        RAV_WARNING("Unsupported network type in connection_info_field");
        return false;
    }
    if (auto& num_addrs = conn.number_of_addresses) {
        if (num_addrs > 1) {
            RAV_WARNING("Unsupported number of addresses in connection_info_field");
            return false;
        }
    }
    return true;
}

}  // namespace

const char* rav::RavennaReceiver::to_string(const ReceiverState state) {
    switch (state) {
        case ReceiverState::idle:
            return "idle";
        case ReceiverState::waiting_for_data:
            return "waiting_for_data";
        case ReceiverState::ok:
            return "ok";
        case ReceiverState::ok_no_consumer:
            return "ok_no_consumer";
        case ReceiverState::inactive:
            return "inactive";
        default:
            return "unknown";
    }
}

nlohmann::json rav::RavennaReceiver::to_json() const {
    nlohmann::json root;
    root["configuration"] = configuration_.to_json();
    return root;
}

std::string rav::RavennaReceiver::StreamParameters::to_string() const {
    return fmt::format(
        "session={}, selected_audio_format={}, packet_time_frames={}", session.to_string(), audio_format.to_string(),
        packet_time_frames
    );
}

nlohmann::json rav::RavennaReceiver::Configuration::to_json() const {
    return nlohmann::json {{"session_name", session_name}, {"delay_frames", delay_frames}, {"enabled", enabled}};
}

tl::expected<rav::RavennaReceiver::ConfigurationUpdate, std::string>
rav::RavennaReceiver::ConfigurationUpdate::from_json(const nlohmann::json& json) {
    try {
        ConfigurationUpdate update {};
        update.session_name = json.at("session_name").get<std::string>();
        update.delay_frames = json.at("delay_frames").get<uint32_t>();
        update.enabled = json.at("enabled").get<bool>();
        return update;
    } catch (const std::exception& e) {
        return tl::unexpected(e.what());
    }
}

rav::RavennaReceiver::RavennaReceiver(
    RavennaRtspClient& rtsp_client, rtp::Receiver& rtp_receiver, const Id id, ConfigurationUpdate initial_config
) :
    rtp_receiver_(rtp_receiver), rtsp_client_(rtsp_client), id_(id), maintenance_timer_(rtp_receiver.get_io_context()) {
    if (!initial_config.delay_frames) {
        initial_config.delay_frames = 480;  // 10ms at 48KHz
    }

    auto result = update_configuration(initial_config);
    if (!result) {
        RAV_ERROR("Failed to update configuration: {}", result.error());
    }
}

rav::RavennaReceiver::~RavennaReceiver() {
    std::ignore = rtsp_client_.unsubscribe_from_all_sessions(this);
    maintenance_timer_.cancel();
}

void rav::RavennaReceiver::on_announced(const RavennaRtspClient::AnnouncedEvent& event) {
    try {
        RAV_ASSERT(event.session_name == configuration_.session_name, "Expecting session_name to match");
        update_sdp(event.sdp);
        RAV_TRACE("SDP updated for session '{}'", configuration_.session_name);
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to process SDP for session '{}': {}", configuration_.session_name, e.what());
    }
}

rav::RavennaReceiver::MediaStream::MediaStream(
    RavennaReceiver& owner, rtp::Receiver& rtp_receiver, rtp::Session session
) :
    owner_(owner), rtp_receiver_(rtp_receiver) {
    parameters_.session = std::move(session);
}

rav::RavennaReceiver::MediaStream::~MediaStream() {
    rtp_receiver_.unsubscribe(this);
}

bool rav::RavennaReceiver::MediaStream::update_parameters(const StreamParameters& new_parameters) {
    bool restart_rtp = false;
    bool restart_audio = false;

    if (parameters_.session != new_parameters.session || parameters_.filter != new_parameters.filter) {
        parameters_.session = new_parameters.session;
        parameters_.filter = new_parameters.filter;
        rtp_receiver_.unsubscribe(this);
        restart_rtp = true;
    }

    if (parameters_.audio_format != new_parameters.audio_format
        || parameters_.packet_time_frames != new_parameters.packet_time_frames) {
        parameters_.audio_format = new_parameters.audio_format;
        parameters_.packet_time_frames = new_parameters.packet_time_frames;
        restart_audio = true;
    }

    for (auto* subscriber : owner_.subscribers_) {
        subscriber->ravenna_receiver_stream_updated(parameters_);
    }

    return restart_rtp || restart_audio;
}

const rav::rtp::Session& rav::RavennaReceiver::MediaStream::get_session() const {
    return parameters_.session;
}

void rav::RavennaReceiver::MediaStream::do_maintenance() {
    // Check if streams became are no longer receiving data
    if (parameters_.state == ReceiverState::ok || parameters_.state == ReceiverState::ok_no_consumer) {
        const auto now = HighResolutionClock::now();
        if ((last_packet_time_ns_ + k_receive_timeout_ms * 1'000'000).value() < now) {
            set_state(ReceiverState::inactive);
        }
    }
}

rav::RavennaReceiver::StreamStats rav::RavennaReceiver::MediaStream::get_stream_stats() const {
    StreamStats s;
    s.packet_stats = packet_stats_.get_total_counts();
    s.packet_interval_stats = packet_interval_stats_.get_stats();
    return s;
}

rav::rtp::PacketStats::Counters rav::RavennaReceiver::MediaStream::get_packet_stats() const {
    return packet_stats_.get_total_counts();
}

rav::SlidingStats::Stats rav::RavennaReceiver::MediaStream::get_packet_interval_stats() const {
    return packet_interval_stats_.get_stats();
}

const rav::RavennaReceiver::StreamParameters& rav::RavennaReceiver::MediaStream::get_parameters() const {
    return parameters_;
}

void rav::RavennaReceiver::MediaStream::start() {
    if (is_running_) {
        return;
    }
    is_running_ = true;
    rtp_ts_.reset();
    packet_stats_.reset();
    rtp_receiver_.subscribe(this, parameters_.session, parameters_.filter);
}

void rav::RavennaReceiver::MediaStream::stop() {
    if (!is_running_) {
        return;
    }
    is_running_ = false;
    rtp_receiver_.unsubscribe(this);
}

void rav::RavennaReceiver::MediaStream::on_rtp_packet(const rtp::Receiver::RtpPacketEvent& rtp_event) {
    // TODO: We should probably discard filtered packets here and not in rtp_receiver. This would also allow us to use a
    // subscriber list without context in rtp_receiver. Alternatively we could add a virtual function to
    // rtp_receiver::subscriber to determine whether the packet should be filtered or not. But since we need to call a
    // virtual function anyway (this one) we might as well filter it here.

    TRACY_ZONE_SCOPED;

    const WrappingUint32 packet_timestamp(rtp_event.packet.timestamp());

    if (!rtp_ts_.has_value()) {
        seq_ = rtp_event.packet.sequence_number();
        rtp_ts_ = rtp_event.packet.timestamp();
        last_packet_time_ns_ = rtp_event.recv_time;
    }

    const auto payload = rtp_event.packet.payload_data();
    if (payload.size_bytes() == 0) {
        RAV_WARNING("Received packet with empty payload");
        return;
    }

    if (payload.size_bytes() > std::numeric_limits<uint16_t>::max()) {
        RAV_WARNING("Payload size exceeds maximum size");
        return;
    }

    if (const auto interval = last_packet_time_ns_.update(rtp_event.recv_time)) {
        TRACY_PLOT("packet interval (ms)", static_cast<double>(*interval) / 1'000'000.0);
        packet_interval_stats_.add(static_cast<double>(*interval) / 1'000'000.0);
    }

    if (packet_interval_throttle_.update()) {
        RAV_TRACE("Packet interval stats: {}", packet_interval_stats_.to_string());
    }

    if (auto lock = owner_.network_thread_reader_.lock_realtime()) {
        if (lock->consumer_active) {
            IntermediatePacket intermediate {};
            intermediate.timestamp = rtp_event.packet.timestamp();
            intermediate.seq = rtp_event.packet.sequence_number();
            intermediate.data_len = static_cast<uint16_t>(payload.size_bytes());
            intermediate.packet_time_frames = parameters_.packet_time_frames;
            std::memcpy(intermediate.data.data(), payload.data(), intermediate.data_len);

            if (!lock->fifo.push(intermediate)) {
                RAV_TRACE("Failed to push packet info FIFO, make receiver inactive");
                lock->consumer_active = false;
                set_state(ReceiverState::ok_no_consumer);
            } else {
                set_state(ReceiverState::ok);
            }
        } else {
            set_state(ReceiverState::ok_no_consumer);
        }

        while (auto seq = lock->packets_too_old.pop()) {
            packet_stats_.mark_packet_too_late(*seq);
        }

        if (const auto stats = packet_stats_.update(rtp_event.packet.sequence_number())) {
            if (auto v = packet_stats_throttle_.update(*stats)) {
                RAV_WARNING("Stats for stream {}: {}", parameters_.session.to_string(), v->to_string());
            }
        }

        if (const auto diff = seq_.update(rtp_event.packet.sequence_number())) {
            if (diff >= 1) {
                // Only call back with monotonically increasing sequence numbers
                for (auto* subscriber : owner_.subscribers_) {
                    subscriber->on_data_received(packet_timestamp);
                }
            }

            if (packet_timestamp - lock->delay_frames >= *rtp_ts_) {
                // Make sure to call with the correct timestamps for the missing packets
                for (uint16_t i = 0; i < *diff; ++i) {
                    for (auto* subscriber : owner_.subscribers_) {
                        subscriber->on_data_ready(
                            packet_timestamp - lock->delay_frames - (*diff - 1u - i) * parameters_.packet_time_frames
                        );
                    }
                }
            }
        }
    }
}

void rav::RavennaReceiver::MediaStream::on_rtcp_packet(const rtp::Receiver::RtcpPacketEvent& rtcp_event) {
    RAV_TRACE(
        "{} for session {} from {}:{}", rtcp_event.packet.to_string(), rtcp_event.session.to_string(),
        rtcp_event.src_endpoint.address().to_string(), rtcp_event.src_endpoint.port()
    );
}

void rav::RavennaReceiver::MediaStream::set_state(const ReceiverState state) {
    if (state == parameters_.state) {
        return;
    }
    parameters_.state = state;
    for (auto* subscriber : owner_.subscribers_) {
        subscriber->ravenna_receiver_stream_updated(parameters_);
    }
}

void rav::RavennaReceiver::update_sdp(const sdp::SessionDescription& sdp) {
    const sdp::MediaDescription* selected_media_description = nullptr;
    const sdp::ConnectionInfoField* selected_connection_info = nullptr;
    std::optional<AudioFormat> selected_audio_format;

    for (auto& media_description : sdp.media_descriptions()) {
        if (media_description.media_type() != "audio") {
            RAV_WARNING("Unsupported media type: {}", media_description.media_type());
            continue;
        }

        if (media_description.protocol() != "RTP/AVP") {
            RAV_WARNING("Unsupported protocol {}", media_description.protocol());
            continue;
        }

        // The first acceptable payload format from the beginning of the list SHOULD be used for the session.
        // https://datatracker.ietf.org/doc/html/rfc8866#name-media-descriptions-m
        selected_audio_format.reset();  // Reset format from previous iteration
        for (auto& format : media_description.formats()) {
            selected_audio_format = format.to_audio_format();
            if (!selected_audio_format) {
                RAV_WARNING("Not a supported audio format: {}", format.to_string());
                continue;
            }
            break;
        }

        if (!selected_audio_format) {
            RAV_WARNING("No supported audio format found");
            continue;
        }

        for (auto& conn : media_description.connection_infos()) {
            if (!is_connection_info_valid(conn)) {
                continue;
            }
            selected_connection_info = &conn;
        }

        selected_media_description = &media_description;
        break;  // We only need the first media description (at least for now)
    }

    if (!selected_media_description) {
        RAV_WARNING("No suitable media description found in SDP");
        return;
    }

    if (!selected_audio_format) {
        RAV_WARNING("No media description with supported audio format available");
        return;
    }

    if (selected_connection_info == nullptr) {
        if (const auto& conn = sdp.connection_info()) {
            if (is_connection_info_valid(*conn)) {
                selected_connection_info = &*conn;
            }
        }
    }

    if (selected_connection_info == nullptr) {
        RAV_WARNING("No suitable connection info found in SDP");
        return;
    }

    RAV_ASSERT(selected_connection_info != nullptr, "Expecting found_connection_info to be set");

    uint16_t packet_time_frames = 0;
    const auto ptime = selected_media_description->ptime();
    if (ptime.has_value()) {
        packet_time_frames = aes67::PacketTime::framecount(*ptime, selected_audio_format->sample_rate);
    }

    if (packet_time_frames == 0) {
        RAV_WARNING("No ptime attribute found, falling back to framecount");
        const auto framecount = selected_media_description->framecount();
        if (!framecount.has_value()) {
            RAV_ERROR("No framecount attribute found");
            return;
        }
        packet_time_frames = *framecount;
    }

    RAV_ASSERT(packet_time_frames > 0, "packet_time_frames must be greater than 0");

    rtp::Session session;

    asio::error_code ec;
    session.rtp_port = selected_media_description->port();
    session.rtcp_port = session.rtp_port + 1;
    session.connection_address = asio::ip::make_address(selected_connection_info->address, ec);
    if (ec) {
        RAV_ERROR("Failed to parse connection address: {}", ec.message());
        return;
    }

    rtp::Filter filter(session.connection_address);

    const auto& source_filters = selected_media_description->source_filters();
    if (!source_filters.empty()) {
        if (filter.add_filters(source_filters) == 0) {
            RAV_WARNING("No suitable source filters found in SDP");
        }
    } else {
        const auto& sdp_source_filters = sdp.source_filters();
        if (!sdp_source_filters.empty()) {
            if (filter.add_filters(sdp_source_filters) == 0) {
                RAV_WARNING("No suitable source filters found in SDP");
            }
        }
    }

    bool do_update_shared_context = false;

    auto notify = false;  // Either notify or restart will trigger a notification
    auto [stream, was_created] = find_or_create_media_stream(session);

    RAV_ASSERT(stream != nullptr, "Expecting stream to be valid");

    StreamParameters stream_parameters;
    stream_parameters.session = session;
    stream_parameters.filter = filter;
    stream_parameters.audio_format = *selected_audio_format;
    stream_parameters.packet_time_frames = packet_time_frames;

    if (stream->update_parameters(stream_parameters)) {
        // The stream parameters have changed, so we need to update the shared context
        do_update_shared_context = true;
        notify = true;
        stream->stop();
    }

    // Delete all streams that are not in the SDP anymore
    for (auto it = media_streams_.begin(); it != media_streams_.end();) {
        if (it->get()->get_session() != session) {
            it = media_streams_.erase(it);
        } else {
            ++it;
        }
    }

    if (configuration_.enabled) {
        for (auto& s : media_streams_) {
            s->start();
        }
    } else {
        for (auto& s : media_streams_) {
            s->stop();
        }
    }

    if (do_update_shared_context || was_created) {
        update_shared_context();
    }

    if (do_update_shared_context || notify) {
        for (auto* subscriber : subscribers_) {
            subscriber->ravenna_receiver_stream_updated(stream->get_parameters());
        }
    }
}

void rav::RavennaReceiver::update_shared_context() {
    if (configuration_.enabled == false) {
        shared_context_.clear();
        return;
    }

    std::optional<AudioFormat> selected_format;
    uint16_t packet_time_frames = 0;

    for (const auto& stream : media_streams_) {
        auto& parameters = stream->get_parameters();
        if (!parameters.audio_format.is_valid()) {
            RAV_WARNING("Invalid audio format");
            return;
        }
        if (!selected_format.has_value()) {
            selected_format = parameters.audio_format;
            packet_time_frames = parameters.packet_time_frames;
        } else if (parameters.audio_format != *selected_format) {
            RAV_WARNING("Audio formats are not the same");
            return;
        }
    }

    if (!selected_format.has_value()) {
        RAV_WARNING("No valid audio format available");
        return;
    }

    const auto bytes_per_frame = selected_format->bytes_per_frame();
    RAV_ASSERT(bytes_per_frame > 0, "bytes_per_frame must be greater than 0");

    auto new_context = std::make_unique<SharedContext>();

    const auto buffer_size_frames = std::max(selected_format->sample_rate * k_buffer_size_ms / 1000, 1024u);
    new_context->receiver_buffer.resize(selected_format->sample_rate * k_buffer_size_ms / 1000, bytes_per_frame);
    new_context->read_buffer.resize(buffer_size_frames * bytes_per_frame);
    const auto buffer_size_packets = buffer_size_frames / packet_time_frames;
    new_context->fifo.resize(buffer_size_packets);
    new_context->packets_too_old.resize(buffer_size_packets);
    new_context->selected_audio_format = *selected_format;
    new_context->delay_frames = configuration_.delay_frames;

    shared_context_.update(std::move(new_context));

    do_maintenance();
}

std::pair<rav::RavennaReceiver::MediaStream*, bool>
rav::RavennaReceiver::find_or_create_media_stream(const rtp::Session& session) {
    for (const auto& stream : media_streams_) {
        if (stream->get_session() == session) {
            return std::make_pair(stream.get(), false);
        }
    }
    const auto& it = media_streams_.emplace_back(std::make_unique<MediaStream>(*this, rtp_receiver_, session));
    return std::make_pair(it.get(), true);
}

void rav::RavennaReceiver::do_maintenance() {
    for (const auto& stream : media_streams_) {
        stream->do_maintenance();
    }

    std::ignore = shared_context_.reclaim();

    maintenance_timer_.expires_after(std::chrono::seconds(1));
    maintenance_timer_.async_wait([this](const asio::error_code ec) {
        if (ec) {
            if (ec == asio::error::operation_aborted) {
                return;
            }
            RAV_ERROR("Timer error: {}", ec.message());
            return;
        }
        do_maintenance();
    });
}

void rav::RavennaReceiver::do_realtime_maintenance() {
    if (auto lock = audio_thread_reader_.lock_realtime()) {
        if (lock->consumer_active.exchange(true) == false) {
            lock->fifo.pop_all();
        }

        while (const auto packet = lock->fifo.pop()) {
            WrappingUint32 packet_timestamp(packet->timestamp);
            if (!lock->first_packet_timestamp) {
                RAV_TRACE("First packet timestamp: {}", packet->timestamp);
                lock->first_packet_timestamp = packet_timestamp;
                lock->receiver_buffer.set_next_ts(packet->timestamp);
                lock->next_ts = packet_timestamp - lock->delay_frames;
            }

            // Determine whether whole packet is too old
            if (packet_timestamp + packet->packet_time_frames <= lock->next_ts) {
                // RAV_WARNING("Packet too late: seq={}, ts={}", packet->seq, packet->timestamp);
                TRACY_MESSAGE("Packet too late - skipping");
                if (!lock->packets_too_old.push(packet->seq)) {
                    RAV_ERROR("Packet not enqueued to packets_too_old");
                }
                continue;
            }

            // Determine whether packet contains outdated data
            if (packet_timestamp < lock->next_ts) {
                RAV_WARNING("Packet partly too late: seq={}, ts={}", packet->seq, packet->timestamp);
                TRACY_MESSAGE("Packet partly too late - not skipping");
                if (!lock->packets_too_old.push(packet->seq)) {
                    RAV_ERROR("Packet not enqueued to packets_too_old");
                }
                // Still process the packet since it contains data that is not outdated
            }

            lock->receiver_buffer.clear_until(packet->timestamp);

            if (!lock->receiver_buffer.write(packet->timestamp, {packet->data.data(), packet->data_len})) {
                RAV_ERROR("Packet not written to buffer");
            }
        }

        TRACY_PLOT("available_frames", static_cast<int64_t>(lock->next_ts.diff(lock->receiver_buffer.get_next_ts())));
    }
}

rav::Id rav::RavennaReceiver::get_id() const {
    return id_;
}

tl::expected<void, std::string> rav::RavennaReceiver::update_configuration(const ConfigurationUpdate& update) {
    // Session name
    if (update.session_name.has_value()) {
        if (update.session_name->empty()) {
            return tl::unexpected("Session name must be valid");
        }
    }

    bool do_update_shared_context = false;
    if (update.delay_frames.has_value() && update.delay_frames != configuration_.delay_frames) {
        configuration_.delay_frames = *update.delay_frames;
        do_update_shared_context = true;
    }

    if (update.enabled.has_value() && update.enabled != configuration_.enabled) {
        configuration_.enabled = *update.enabled;
        do_update_shared_context = true;
    }

    if (update.session_name.has_value() && update.session_name != configuration_.session_name) {
        configuration_.session_name = *update.session_name;

        std::ignore = rtsp_client_.unsubscribe_from_all_sessions(this);

        RAV_ASSERT(configuration_.session_name != "", "Session name must not be empty");

        if (!rtsp_client_.subscribe_to_session(this, configuration_.session_name)) {
            RAV_ERROR("Failed to subscribe to session '{}'", configuration_.session_name);
            return tl::unexpected("Failed to subscribe to session");
        }

        // A restart will happen when the SDP is received
    }

    if (do_update_shared_context) {
        update_shared_context();
    }

    for (auto& stream : media_streams_) {
        configuration_.enabled ? stream->start() : stream->stop();
    }

    for (auto* subscriber : subscribers_) {
        subscriber->ravenna_receiver_configuration_updated(get_id(), configuration_);
    }

    return {};
}

const rav::RavennaReceiver::Configuration& rav::RavennaReceiver::get_configuration() const {
    return configuration_;
}

bool rav::RavennaReceiver::subscribe(Subscriber* subscriber) {
    if (subscribers_.add(subscriber)) {
        subscriber->ravenna_receiver_configuration_updated(get_id(), configuration_);
        for (const auto& stream : media_streams_) {
            subscriber->ravenna_receiver_stream_updated(stream->get_parameters());
        }
        return true;
    }
    return false;
}

bool rav::RavennaReceiver::unsubscribe(Subscriber* subscriber) {
    return subscribers_.remove(subscriber);
}

std::optional<rav::sdp::SessionDescription> rav::RavennaReceiver::get_sdp() const {
    return rtsp_client_.get_sdp_for_session(configuration_.session_name);
}

std::optional<std::string> rav::RavennaReceiver::get_sdp_text() const {
    return rtsp_client_.get_sdp_text_for_session(configuration_.session_name);
}

std::optional<uint32_t> rav::RavennaReceiver::read_data_realtime(
    uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    if (auto lock = audio_thread_reader_.lock_realtime()) {
        RAV_ASSERT_EXCLUSIVE_ACCESS(realtime_access_guard_);
        RAV_ASSERT(buffer_size != 0, "Buffer size must be greater than 0");
        RAV_ASSERT(buffer != nullptr, "Buffer must not be nullptr");

        do_realtime_maintenance();

        if (buffer_size > lock->read_buffer.size()) {
            RAV_WARNING("Buffer size is larger than the read buffer size");
            return std::nullopt;
        }

        if (at_timestamp.has_value()) {
            lock->next_ts = *at_timestamp;
        }

        const auto num_frames = static_cast<uint32_t>(buffer_size) / lock->selected_audio_format.bytes_per_frame();

        const auto read_at = lock->next_ts.value();
        if (!lock->receiver_buffer.read(read_at, buffer, buffer_size)) {
            return std::nullopt;
        }

        lock->next_ts += num_frames;

        return read_at;
    }

    return std::nullopt;
}

std::optional<uint32_t> rav::RavennaReceiver::read_audio_data_realtime(
    AudioBufferView<float> output_buffer, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    RAV_ASSERT(output_buffer.is_valid(), "Buffer must be valid");

    if (auto lock = audio_thread_reader_.lock_realtime()) {
        const auto format = lock->selected_audio_format;

        if (format.byte_order != AudioFormat::ByteOrder::be) {
            RAV_ERROR("Unexpected byte order");
            return std::nullopt;
        }

        if (format.ordering != AudioFormat::ChannelOrdering::interleaved) {
            RAV_ERROR("Unexpected channel ordering");
            return std::nullopt;
        }

        if (format.num_channels != output_buffer.num_channels()) {
            RAV_ERROR("Channel mismatch");
            return std::nullopt;
        }

        auto& buffer = lock->read_buffer;
        const auto read_at =
            read_data_realtime(buffer.data(), output_buffer.num_frames() * format.bytes_per_frame(), at_timestamp);

        if (!read_at.has_value()) {
            return std::nullopt;
        }

        if (format.encoding == AudioEncoding::pcm_s16) {
            const auto ok = AudioData::convert<
                int16_t, AudioData::ByteOrder::Be, AudioData::Interleaving::Interleaved, float,
                AudioData::ByteOrder::Ne>(
                reinterpret_cast<int16_t*>(buffer.data()), output_buffer.num_frames(), output_buffer.num_channels(),
                output_buffer.data()
            );
            if (!ok) {
                RAV_WARNING("Failed to convert audio data");
            }
        } else if (format.encoding == AudioEncoding::pcm_s24) {
            const auto ok = AudioData::convert<
                int24_t, AudioData::ByteOrder::Be, AudioData::Interleaving::Interleaved, float,
                AudioData::ByteOrder::Ne>(
                reinterpret_cast<int24_t*>(buffer.data()), output_buffer.num_frames(), output_buffer.num_channels(),
                output_buffer.data()
            );
            if (!ok) {
                RAV_WARNING("Failed to convert audio data");
            }
        } else {
            RAV_ERROR("Unsupported encoding");
            return std::nullopt;
        }

        return read_at;
    }

    return std::nullopt;
}

rav::RavennaReceiver::StreamStats rav::RavennaReceiver::get_stream_stats() const {
    if (media_streams_.empty()) {
        return {};
    }
    return media_streams_.front()->get_stream_stats();
}

rav::rtp::PacketStats::Counters rav::RavennaReceiver::get_packet_stats() const {
    if (media_streams_.empty()) {
        return {};
    }
    return media_streams_.front()->get_packet_stats();
}

rav::SlidingStats::Stats rav::RavennaReceiver::get_packet_interval_stats() const {
    if (media_streams_.empty()) {
        return {};
    }
    return media_streams_.front()->get_packet_interval_stats();
}
