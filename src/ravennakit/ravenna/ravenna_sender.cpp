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
#include "ravennakit/ravenna/ravenna_sender.hpp"

#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/core/audio/audio_data.hpp"
#include "ravennakit/nmos/detail/nmos_media_types.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#if RAV_WINDOWS
    #include <timeapi.h>
#endif

#if RAV_WINDOWS
    #pragma comment(lib, "winmm.lib")
#endif

nlohmann::json rav::RavennaSender::Destination::to_json() const {
    return nlohmann::json {
        {"interface_by_rank", interface_by_rank.value()},
        {"address", endpoint.address().to_string()},
        {"port", endpoint.port()},
        {"enabled", enabled},
    };
}

tl::expected<rav::RavennaSender::Destination, std::string>
rav::RavennaSender::Destination::from_json(const nlohmann::json& json) {
    try {
        Destination destination {};
        destination.interface_by_rank = Rank(json.at("interface_by_rank").get<uint8_t>());
        destination.endpoint = boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address_v4(json.at("address").get<std::string>()), json.at("port").get<uint16_t>()
        );
        destination.enabled = json.at("enabled").get<bool>();
        return destination;
    } catch (const std::exception& e) {
        return tl::unexpected(e.what());
    }
}

nlohmann::json rav::RavennaSender::Configuration::to_json() const {
    nlohmann::json::array_t destinations_array;
    for (const auto& dst : destinations) {
        destinations_array.push_back(dst.to_json());
    }
    return nlohmann::json {
        {"session_name", session_name},
        {"destinations", destinations_array},
        {"ttl", ttl},
        {"payload_type", payload_type},
        {"audio_format", audio_format.to_json()},
        {"packet_time", packet_time.to_json()},
        {"enabled", enabled}
    };
}

tl::expected<rav::RavennaSender::Configuration, std::string>
rav::RavennaSender::Configuration::from_json(const nlohmann::json& json) {
    try {
        Configuration config {};
        config.session_name = json.at("session_name").get<std::string>();

        std::vector<Destination> destinations;
        for (auto& dst : json.at("destinations")) {
            destinations.push_back(Destination::from_json(dst).value());
        }
        config.destinations = std::move(destinations);
        config.ttl = json.at("ttl").get<int32_t>();
        config.payload_type = json.at("payload_type").get<uint8_t>();
        auto audio_format = AudioFormat::from_json(json.at("audio_format"));
        if (!audio_format) {
            return tl::unexpected(audio_format.error());
        }
        config.audio_format = *audio_format;
        const auto packet_time = aes67::PacketTime::from_json(json.at("packet_time"));
        if (!packet_time) {
            return tl::unexpected("Invalid packet time");
        }
        config.packet_time = *packet_time;
        config.enabled = json.at("enabled").get<bool>();
        return config;
    } catch (const std::exception& e) {
        return tl::unexpected(e.what());
    }
}

rav::RavennaSender::RavennaSender(
    boost::asio::io_context& io_context, dnssd::Advertiser& advertiser, rtsp::Server& rtsp_server,
    ptp::Instance& ptp_instance, const Id id, const uint32_t session_id
) :
    io_context_(io_context),
    advertiser_(advertiser),
    rtsp_server_(rtsp_server),
    ptp_instance_(ptp_instance),
    id_(id),
    session_id_(session_id),
    timer_(io_context) {
    RAV_ASSERT(id_.is_valid(), "Sender ID must be valid");
    RAV_ASSERT(session_id != 0, "Session ID must be valid");

    nmos_source_.id = boost::uuids::random_generator()();
    nmos_source_.clock_name = nmos::Node::k_ptp_clock_name;

    nmos_flow_.id = boost::uuids::random_generator()();
    nmos_flow_.source_id = nmos_source_.id;

    nmos_sender_.id = boost::uuids::random_generator()();
    nmos_sender_.flow_id = nmos_flow_.id;

    std::vector<Destination> destinations;
    destinations.emplace_back(Destination {Rank::primary(), {boost::asio::ip::address_v4::any(), 5004}, true});
    destinations.emplace_back(Destination {Rank::secondary(), {boost::asio::ip::address_v4::any(), 5004}, true});
    configuration_.destinations = std::move(destinations);

    if (!ptp_instance_.subscribe(this)) {
        RAV_ERROR("Failed to subscribe to PTP instance");
    }

#if RAV_WINDOWS
    timeBeginPeriod(1);
#endif
}

rav::RavennaSender::~RavennaSender() {
    stop_timer();

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

    if (nmos_node_ != nullptr) {
        if (!nmos_node_->remove_sender(nmos_sender_.id)) {
            RAV_ERROR("Failed to remove NMOS receiver with ID: {}", boost::uuids::to_string(nmos_sender_.id));
        }
        if (!nmos_node_->remove_flow(nmos_flow_.id)) {
            RAV_ERROR("Failed to remove NMOS flow with ID: {}", boost::uuids::to_string(nmos_flow_.id));
        }
        if (!nmos_node_->remove_source(nmos_source_.id)) {
            RAV_ERROR("Failed to remove NMOS source with ID: {}", boost::uuids::to_string(nmos_source_.id));
        }
    }
}

rav::Id rav::RavennaSender::get_id() const {
    return id_;
}

const boost::uuids::uuid& rav::RavennaSender::get_uuid() const {
    return nmos_sender_.id;
}

uint32_t rav::RavennaSender::get_session_id() const {
    return session_id_;
}

tl::expected<void, std::string> rav::RavennaSender::set_configuration(Configuration config) {
    std::ignore = shared_context_.reclaim();  // TODO: Do somewhere else, maybe on a timer or something.

    // Validate the configuration

    if (config.session_name.empty()) {
        return tl::unexpected("Session name cannot be empty");
    }

    if (config.destinations.empty()) {
        return tl::unexpected("no destinations set");
    }

    if (config.enabled) {
        int num_enabled_destinations = 0;
        for (const auto& dst : config.destinations) {
            if (!dst.enabled) {
                continue;
            }
            if (!dst.endpoint.address().is_unspecified() && !dst.endpoint.address().is_multicast()) {
                return tl::unexpected(
                    fmt::format("{} address must be multicast", dst.interface_by_rank.to_ordinal_latin())
                );
            }
            num_enabled_destinations++;
            auto* iface = network_interface_config_.get_interface(dst.interface_by_rank);
            if (iface == nullptr) {
                return tl::unexpected(fmt::format("{} interface not set", dst.interface_by_rank.to_ordinal_latin()));
            }
            if (dst.endpoint.address().is_unspecified()) {
                return tl::unexpected(
                    fmt::format("{} destination address is unspecified", dst.interface_by_rank.to_ordinal_latin())
                );
            }
            if (dst.endpoint.port() == 0) {
                return tl::unexpected(
                    fmt::format("{} destination port is 0", dst.interface_by_rank.to_ordinal_latin())
                );
            }
        }

        if (num_enabled_destinations == 0) {
            return tl::unexpected("no enabled destinations");
        }

        if (config.ttl <= 0) {
            return tl::unexpected("Invalid TTL");
        }

        const auto& audio_format = config.audio_format;
        if (!audio_format.is_valid()) {
            return tl::unexpected("Invalid audio format");
        }
        if (audio_format.ordering != AudioFormat::ChannelOrdering::interleaved) {
            return tl::unexpected("Only interleaved audio formats are supported");
        }
        if (audio_format.byte_order != AudioFormat::ByteOrder::be) {
            return tl::unexpected("Only big endian audio formats are supported");
        }
        if (!config.packet_time.is_valid()) {
            return tl::unexpected("Invalid packet time");
        }
    }

    // Determine changes

    bool update_advertisement = false;
    bool announce = false;
    bool update_nmos = false;

    if (config.enabled != configuration_.enabled) {
        update_advertisement = true;
        announce = true;
        update_nmos = true;
    }

    if (config.session_name != configuration_.session_name) {
        update_advertisement = true;
        announce = true;
        update_nmos = true;
    }

    if (config.destinations != configuration_.destinations) {
        announce = true;
        update_nmos = true;
    }

    if (config.ttl != configuration_.ttl) {
        announce = true;
        // TODO: Update socket option for TTL. Probably both multicast and unicast in one go (which are separate opts).
    }

    if (config.payload_type != configuration_.payload_type) {
        announce = true;
    }

    if (config.audio_format != configuration_.audio_format) {
        announce = true;
        update_nmos = true;
    }

    if (config.packet_time != configuration_.packet_time) {
        announce = true;
    }

    // Implement the changes

    configuration_ = std::move(config);
    update_state(update_advertisement, announce, update_nmos);
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

bool rav::RavennaSender::unsubscribe(const Subscriber* subscriber) {
    return subscribers_.remove(subscriber);
}

void rav::RavennaSender::set_nmos_node(nmos::Node* nmos_node) {
    if (nmos_node_ == nmos_node) {
        return;
    }
    nmos_node_ = nmos_node;
    if (nmos_node_ != nullptr) {
        RAV_ASSERT(nmos_sender_.is_valid(), "NMOS receiver must be valid at this point");
        RAV_ASSERT(nmos_flow_.is_valid(), "NMOS flow must be valid at this point");
        RAV_ASSERT(nmos_source_.is_valid(), "NMOS source must be valid at this point");
        if (!nmos_node_->add_or_update_source({nmos_source_})) {
            RAV_ERROR("Failed to add NMOS source with ID: {}", boost::uuids::to_string(nmos_source_.id));
        }
        if (!nmos_node_->add_or_update_flow({nmos_flow_})) {
            RAV_ERROR("Failed to add NMOS flow with ID: {}", boost::uuids::to_string(nmos_flow_.id));
        }
        if (!nmos_node_->add_or_update_sender(nmos_sender_)) {
            RAV_ERROR("Failed to add NMOS sender with ID: {}", boost::uuids::to_string(nmos_sender_.id));
        }
    }
}

void rav::RavennaSender::set_nmos_device_id(const boost::uuids::uuid& device_id) {
    nmos_source_.device_id = device_id;
    nmos_flow_.device_id = device_id;
    nmos_sender_.device_id = device_id;
}

uint32_t rav::RavennaSender::get_framecount() const {
    return configuration_.packet_time.framecount(configuration_.audio_format.sample_rate);
}

bool rav::RavennaSender::send_data_realtime(const BufferView<const uint8_t> buffer, const uint32_t timestamp) {
    if (!get_local_clock().is_locked()) {
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

            if (lock->outgoing_data.push(packet)) {
                // RAV_TRACE(
                // "Scheduled RTP packet: {} bytes, timestamp: {}, seq: {}", lock->rtp_packet_buffer.size(),
                // packet.rtp_timestamp, rtp_packet.get_sequence_number().value()
                // );
            } else {
                RAV_ERROR(
                    "Failed to schedule RTP packet: {} bytes, timestamp: {}, seq: {}", lock->rtp_packet_buffer.size(),
                    packet.rtp_timestamp, rtp_packet.get_sequence_number().value()
                );
            }

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

void rav::RavennaSender::set_network_interface_config(NetworkInterfaceConfig network_interface_config) {
    if (network_interface_config_ == network_interface_config) {
        return;  // No change in network interface configuration
    }
    network_interface_config_ = std::move(network_interface_config);
    update_state(false, false, true);
}

const rav::nmos::SourceAudio& rav::RavennaSender::get_nmos_source() const {
    return nmos_source_;
}

const rav::nmos::FlowAudioRaw& rav::RavennaSender::get_nmos_flow() const {
    return nmos_flow_;
}

const rav::nmos::Sender& rav::RavennaSender::get_nmos_sender() const {
    return nmos_sender_;
}

nlohmann::json rav::RavennaSender::to_json() const {
    nlohmann::json root;
    root["session_id"] = session_id_;
    root["configuration"] = configuration_.to_json();
    root["nmos_sender_uuid"] = boost::uuids::to_string(nmos_sender_.id);
    root["nmos_source_uuid"] = boost::uuids::to_string(nmos_source_.id);
    root["nmos_flow_uuid"] = boost::uuids::to_string(nmos_flow_.id);
    return root;
}

boost::json::object rav::RavennaSender::to_boost_json() const {
    return {
        {"session_id", session_id_},
        {"configuration", boost::json::value_from(configuration_)},
        {"nmos_sender_uuid", boost::uuids::to_string(nmos_sender_.id)},
        {"nmos_source_uuid", boost::uuids::to_string(nmos_source_.id)},
        {"nmos_flow_uuid", boost::uuids::to_string(nmos_flow_.id)},
    };
}

tl::expected<void, std::string> rav::RavennaSender::restore_from_json(const nlohmann::json& json) {
    try {
        auto config = Configuration::from_json(json.at("configuration"));
        if (!config) {
            return tl::unexpected(config.error());
        }

        const auto session_id = json.at("session_id").get<uint32_t>();
        if (session_id == 0) {
            return tl::unexpected("Session ID must be valid");
        }

        const auto nmos_source_uuid = boost::uuids::string_generator()(json.at("nmos_source_uuid").get<std::string>());
        const auto nmos_flow_uuid = boost::uuids::string_generator()(json.at("nmos_flow_uuid").get<std::string>());
        const auto nmos_sender_uuid = boost::uuids::string_generator()(json.at("nmos_sender_uuid").get<std::string>());

        auto result = set_configuration(*config);
        if (!result) {
            return tl::unexpected(fmt::format("Failed to update sender configuration: {}", result.error()));
        }

        session_id_ = session_id;

        nmos_source_.id = nmos_source_uuid;

        nmos_flow_.id = nmos_flow_uuid;
        nmos_flow_.source_id = nmos_source_.id;

        nmos_sender_.id = nmos_sender_uuid;
        nmos_sender_.flow_id = nmos_flow_.id;
    } catch (const std::exception& e) {
        return tl::unexpected(fmt::format("Failed to restore RavennaSender from JSON: {}", e.what()));
    }
    return {};
}

void rav::RavennaSender::on_request(const rtsp::Connection::RequestEvent event) const {
    rtsp::Response error_response(204, "No Content");
    if (const auto* cseq = event.rtsp_request.rtsp_headers.get("cseq")) {
        error_response.rtsp_headers.set(*cseq);
    }

    const auto sdp = build_sdp();  // Should the SDP be cached and updated on changes?
    if (!sdp) {
        RAV_ERROR("Failed to build SDP: {}", sdp.error());
        event.rtsp_connection.async_send_response(error_response);
        return;
    }

    const auto sdp_debug_string = sdp->to_string("\n");
    if (!sdp_debug_string) {
        RAV_ERROR("Failed to build SDP debug string: {}", sdp_debug_string.error());
        event.rtsp_connection.async_send_response(error_response);
        return;
    }

    RAV_TRACE("SDP:\n{}", sdp_debug_string.value());

    const auto sdp_string = sdp->to_string("\r\n");
    if (!sdp_string) {
        RAV_ERROR("Failed to encode SDP");
        event.rtsp_connection.async_send_response(error_response);
        return;
    }

    auto response = rtsp::Response(200, "OK", *sdp_string);
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
    if (!rtsp_path_by_name_.empty()) {
        send_announce();
    }
}

void rav::RavennaSender::send_announce() const {
    if (network_interface_config_.empty()) {
        RAV_ERROR("No interface addresses set");
        return;
    }

    auto sdp = build_sdp();
    if (!sdp) {
        RAV_ERROR("Failed to encode SDP: {}", sdp.error());
        return;
    }

    auto sdp_string = sdp->to_string("\r\n");
    if (!sdp_string) {
        RAV_ERROR("Failed to encode SDP");
        return;
    }

    const auto interface_address_string =
        network_interface_config_.get_interface_ipv4_address(Rank::primary()).to_string();

    rtsp::Request request;
    request.method = "ANNOUNCE";
    request.rtsp_headers.set("content-type", "application/sdp");
    request.data = std::move(*sdp_string);
    request.uri =
        Uri::encode("rtsp", interface_address_string + ":" + std::to_string(rtsp_server_.port()), rtsp_path_by_name_);
    std::ignore = rtsp_server_.send_request(rtsp_path_by_name_, request);

    request.uri =
        Uri::encode("rtsp", interface_address_string + ":" + std::to_string(rtsp_server_.port()), rtsp_path_by_id_);
    std::ignore = rtsp_server_.send_request(rtsp_path_by_id_, request);
}

tl::expected<rav::sdp::SessionDescription, std::string> rav::RavennaSender::build_sdp() const {
    if (configuration_.session_name.empty()) {
        return tl::unexpected("Session name not set");
    }

    if (configuration_.destinations.empty()) {
        return tl::unexpected("No destinations set");
    }

    const auto sdp_format = sdp::Format::from_audio_format(configuration_.audio_format);
    if (!sdp_format) {
        return tl::unexpected("Invalid audio format");
    }

    if (network_interface_config_.empty()) {
        return tl::unexpected("No interface addresses set");
    }

    // Reference clock
    const sdp::ReferenceClock ref_clock {
        sdp::ReferenceClock::ClockSource::ptp, sdp::ReferenceClock::PtpVersion::IEEE_1588_2008,
        grandmaster_identity_.to_string(), clock_domain_
    };

    // Media clock
    // ST 2110-30:2017 defines a constraint to use a zero offset exclusively.
    sdp::MediaClockSource media_clk {sdp::MediaClockSource::ClockMode::direct, 0, {}};

    sdp::RavennaClockDomain clock_domain {sdp::RavennaClockDomain::SyncSource::ptp_v2, clock_domain_};

    // Origin
    const sdp::OriginField origin {
        "-",
        std::to_string(session_id_),
        0,
        sdp::NetwType::internet,
        sdp::AddrType::ipv4,
        network_interface_config_.get_interface_ipv4_address(Rank::primary()).to_string(),
    };

    sdp::SessionDescription sdp;
    sdp.set_origin(origin);
    sdp.set_session_name(configuration_.session_name);
    sdp.set_clock_domain(clock_domain);
    sdp.set_ref_clock(ref_clock);
    sdp.set_media_clock(media_clk);

    auto num_active_destinations = 0;
    for (auto& dst : configuration_.destinations) {
        if (dst.enabled) {
            num_active_destinations++;
        }
    }

    sdp::Group group;

    for (auto& dst : configuration_.destinations) {
        if (!dst.enabled) {
            continue;
        }

        if (dst.endpoint.address().is_unspecified()) {
            return tl::unexpected("Destination endpoint is unspecified");
        }

        std::string dst_address_str = dst.endpoint.address().to_string();

        // Connection info
        const sdp::ConnectionInfoField connection_info {
            sdp::NetwType::internet, sdp::AddrType::ipv4, dst_address_str, 15, {}
        };

        auto addr = network_interface_config_.get_interface_ipv4_address(dst.interface_by_rank);
        if (addr.is_unspecified()) {
            return tl::unexpected(fmt::format("No interface address for rank {}", dst.interface_by_rank.value()));
        }

        RAV_ASSERT(!addr.is_multicast(), "Interface address must not be multicast");

        // Source filter
        sdp::SourceFilter filter(
            sdp::FilterMode::include, sdp::NetwType::internet, sdp::AddrType::ipv4, dst_address_str, {addr.to_string()}
        );

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

        if (num_active_destinations > 1) {
            media.set_mid(dst.interface_by_rank.to_ordinal_latin());
            group.add_tag(dst.interface_by_rank.to_ordinal_latin());
        }

        sdp.add_media_description(std::move(media));
    }

    if (!group.empty()) {
        sdp.set_group(group);
    }

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
    timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (ec) {
            RAV_ERROR("Timer error: {}", ec.message());
            return;
        }
        send_outgoing_data();
        start_timer();
    });
}

void rav::RavennaSender::stop_timer() {
    timer_.expires_after(std::chrono::hours(24));
    timer_.async_wait([](boost::system::error_code) {});
    const auto num_canceled = timer_.cancel();
    if (num_canceled == 0) {
        RAV_WARNING("No timer handlers canceled");
    }
}

void rav::RavennaSender::send_outgoing_data() {
    if (auto lock = send_outgoing_data_reader_.lock_realtime()) {
        TRACY_ZONE_SCOPED;

        // Allow to send 100 extra packets which come in during the loop, but otherwise keep the loop bounded
        for (size_t i = 0; i < lock->outgoing_data.size() + 100; ++i) {
            const auto packet = lock->outgoing_data.pop();

            if (!packet.has_value()) {
                return;  // Nothing to do here
            }

            RAV_ASSERT(packet->payload_size_bytes <= aes67::constants::k_max_payload, "Payload size exceeds maximum");

            for (auto& dst : configuration_.destinations) {
                if (dst.enabled) {
                    auto it = rtp_senders_.find(dst.interface_by_rank);
                    if (it != rtp_senders_.end()) {
                        it->second.send_to(packet->payload.data(), packet->payload_size_bytes, dst.endpoint);
                    }
                }
            }
        }
    }
}

void rav::RavennaSender::update_shared_context() {
    // TODO: Implement proper SSRC generation
    const auto ssrc = static_cast<uint32_t>(Random().get_random_int(0, std::numeric_limits<int>::max()));

    auto new_context = std::make_unique<SharedContext>();
    const auto audio_format = configuration_.audio_format;
    const auto packet_size_frames = get_framecount();
    const auto packet_size_bytes = packet_size_frames * audio_format.bytes_per_frame();
    new_context->audio_format = audio_format;
    new_context->packet_time_frames = get_framecount();
    new_context->rtp_packet.payload_type(configuration_.payload_type);
    new_context->rtp_packet.ssrc(ssrc);
    new_context->outgoing_data.resize(k_buffer_num_packets);
    new_context->intermediate_audio_buffer.resize(k_max_num_frames * audio_format.bytes_per_frame());
    new_context->rtp_packet_buffer = ByteBuffer(packet_size_bytes);
    new_context->intermediate_send_buffer.resize(packet_size_bytes);
    new_context->rtp_buffer.resize(k_max_num_frames, audio_format.bytes_per_frame());
    new_context->rtp_buffer.set_ground_value(audio_format.ground_value());

    shared_context_.update(std::move(new_context));
}

void rav::RavennaSender::generate_auto_addresses_if_needed() {
    // Generate a multicast addresses if not set

    bool changed = false;
    for (auto& dst : configuration_.destinations) {
        if (dst.endpoint.address().is_unspecified()) {
            const auto iface = network_interface_config_.get_interface(dst.interface_by_rank);
            if (iface != nullptr) {
                auto addr = network_interface_config_.get_interface_ipv4_address(dst.interface_by_rank);
                if (addr.is_unspecified()) {
                    RAV_WARNING("Invalid interface address for rank {}", dst.interface_by_rank.value());
                    continue;
                }
                // Construct a multicast address from the interface address
                const auto interface_address_bytes = addr.to_bytes();
                dst.endpoint.address(
                    boost::asio::ip::address_v4(
                        {239, interface_address_bytes[2], interface_address_bytes[3],
                         static_cast<uint8_t>(id_.value() % 0xff)}
                    )
                );
                changed = true;

                RAV_TRACE(
                    "Generated {} multicast address {}", dst.interface_by_rank.to_ordinal_latin(),
                    dst.endpoint.address().to_string()
                );
            }
        }
    }
    if (changed) {
        for (auto* subscriber : subscribers_) {
            subscriber->ravenna_sender_configuration_updated(id_, configuration_);
        }
    }
}

void rav::RavennaSender::update_rtp_senders() {
    // Ensure RTP senders exist for enabled ports and for which an interface address exists

    // Create RTP senders for enabled destinations
    for (auto& dst : configuration_.destinations) {
        if (dst.enabled && rtp_senders_.find(dst.interface_by_rank) == rtp_senders_.end()) {
            auto addr = network_interface_config_.get_interface_ipv4_address(dst.interface_by_rank);
            if (addr.is_unspecified()) {
                RAV_TRACE("{} interface not set", rav::string_to_upper(dst.interface_by_rank.to_ordinal_latin(), 1));
                continue;
            }
            rtp_senders_.insert({dst.interface_by_rank, rtp::Sender {io_context_, addr}});
        }
    }

    // Remove RTP senders for disabled or non-existent destinations
    for (auto it = rtp_senders_.begin(); it != rtp_senders_.end();) {
        bool has_at_least_one_enabled_destination = false;
        for (auto& dst : configuration_.destinations) {
            if (dst.interface_by_rank == it->first && dst.enabled) {
                has_at_least_one_enabled_destination = true;
                break;
            }
        }
        if (!has_at_least_one_enabled_destination) {
            RAV_TRACE("Removing RTP sender for rank {}", it->first.value());
            it = rtp_senders_.erase(it++);
        } else {
            ++it;
        }
    }
}

void rav::RavennaSender::update_state(const bool update_advertisement, const bool announce, const bool update_nmos) {
    generate_auto_addresses_if_needed();
    update_rtp_senders();

    if (update_advertisement || !configuration_.enabled) {
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

    if (update_nmos) {
        const auto& audio_format = configuration_.audio_format;

        nmos_sender_.label = configuration_.session_name;
        nmos_flow_.label = configuration_.session_name;
        nmos_source_.label = configuration_.session_name;
        nmos_sender_.transport = "urn:x-nmos:transport:rtp.mcast";
        nmos_sender_.subscription.active = configuration_.enabled;
        nmos_sender_.interface_bindings.clear();
        for (const auto& dst : configuration_.destinations) {
            if (dst.enabled) {
                const auto iface = network_interface_config_.get_interface(dst.interface_by_rank);
                if (iface != nullptr) {
                    nmos_sender_.interface_bindings.push_back(*iface);
                } else {
                    RAV_WARNING("No interface for rank {}", dst.interface_by_rank.to_ordinal_latin());
                }
            }
        }

        nmos_source_.channels.resize(audio_format.num_channels);
        for (uint32_t i = 0; i < audio_format.num_channels; ++i) {
            nmos_source_.channels[i].label = fmt::format("Channel {}", i + 1);
        }

        nmos_flow_.media_type = nmos::audio_encoding_to_nmos_media_type(audio_format.encoding);
        nmos_flow_.sample_rate = {static_cast<int>(audio_format.sample_rate), 1};
        nmos_flow_.bit_depth = audio_format.bytes_per_sample() * 8;

        if (nmos_node_ != nullptr) {
            if (!nmos_node_->add_or_update_source({nmos_source_})) {
                RAV_ERROR("Failed to add NMOS source with ID: {}", boost::uuids::to_string(nmos_source_.id));
            }
            if (!nmos_node_->add_or_update_flow({nmos_flow_})) {
                RAV_ERROR("Failed to add NMOS flow with ID: {}", boost::uuids::to_string(nmos_flow_.id));
            }
            if (!nmos_node_->add_or_update_sender(nmos_sender_)) {
                RAV_ERROR("Failed to add NMOS sender with ID: {}", boost::uuids::to_string(nmos_sender_.id));
            }
        }
    }

    for (const auto& subscriber : subscribers_) {
        subscriber->ravenna_sender_configuration_updated(id_, configuration_);
    }

    if (!configuration_.enabled) {
        stop_timer();
        shared_context_.clear();
        return;  // Done here
    }

    start_timer();

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

    update_shared_context();
}

void rav::tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const RavennaSender::Destination& destination
) {
    jv = {
        {"interface_by_rank", destination.interface_by_rank.value()},
        {"address", destination.endpoint.address().to_string()},
        {"port", destination.endpoint.port()},
        {"enabled", destination.enabled},
    };
}

void rav::tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const RavennaSender::Configuration& config
) {
    jv = {
        {"session_name", config.session_name},
        {"destinations", boost::json::value_from(config.destinations)},
        {"ttl", config.ttl},
        {"payload_type", config.payload_type},
        {"packet_time", boost::json::value_from(config.packet_time)},
        {"audio_format", boost::json::value_from(config.audio_format)},
        {"enabled", config.enabled},
    };
}
