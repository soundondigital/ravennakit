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
#include "ravennakit/nmos/detail/nmos_media_types.hpp"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#if RAV_WINDOWS
    #include <timeapi.h>
#endif

#if RAV_WINDOWS
    #pragma comment(lib, "winmm.lib")
#endif

rav::RavennaSender::RavennaSender(
    rtp::AudioSender& rtp_audio_sender, dnssd::Advertiser& advertiser, rtsp::Server& rtsp_server,
    ptp::Instance& ptp_instance, const Id id, const uint32_t session_id
) :
    rtp_audio_sender_(rtp_audio_sender),
    advertiser_(advertiser),
    rtsp_server_(rtsp_server),
    ptp_instance_(ptp_instance),
    id_(id),
    session_id_(session_id) {
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
}

rav::RavennaSender::~RavennaSender() {
    std::ignore = rtp_audio_sender_.remove_writer(id_);

    if (!ptp_instance_.unsubscribe(this)) {
        RAV_ERROR("Failed to unsubscribe from PTP instance");
    }

    if (advertisement_id_.is_valid()) {
        advertiser_.unregister_service(advertisement_id_);
    }

    rtsp_server_.unregister_handler(this);

    if (nmos_node_ != nullptr) {
        if (!nmos_node_->remove_sender(&nmos_sender_)) {
            RAV_ERROR("Failed to remove NMOS receiver with ID: {}", boost::uuids::to_string(nmos_sender_.id));
        }
        if (!nmos_node_->remove_flow(&nmos_flow_)) {
            RAV_ERROR("Failed to remove NMOS flow with ID: {}", boost::uuids::to_string(nmos_flow_.id));
        }
        if (!nmos_node_->remove_source(&nmos_source_)) {
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
    // Validate the configuration

    std::ignore = generate_auto_addresses_if_needed(config.destinations);

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
            auto* iface = network_interface_config_.get_interface_for_rank(dst.interface_by_rank.value());
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
        if (!nmos_node_->add_or_update_source(&nmos_source_)) {
            RAV_ERROR("Failed to add NMOS source with ID: {}", boost::uuids::to_string(nmos_source_.id));
        }
        if (!nmos_node_->add_or_update_flow(&nmos_flow_)) {
            RAV_ERROR("Failed to add NMOS flow with ID: {}", boost::uuids::to_string(nmos_flow_.id));
        }
        if (!nmos_node_->add_or_update_sender(&nmos_sender_)) {
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

boost::json::object rav::RavennaSender::to_boost_json() const {
    return {
        {"session_id", session_id_},
        {"configuration", boost::json::value_from(configuration_)},
        {"nmos_sender_uuid", boost::uuids::to_string(nmos_sender_.id)},
        {"nmos_source_uuid", boost::uuids::to_string(nmos_source_.id)},
        {"nmos_flow_uuid", boost::uuids::to_string(nmos_flow_.id)},
    };
}

tl::expected<void, std::string> rav::RavennaSender::restore_from_json(const boost::json::value& json) {
    try {
        auto config = boost::json::try_value_to<Configuration>(json.at("configuration"));
        if (!config) {
            return tl::unexpected(config.error().message());
        }

        const auto session_id = json.at("session_id").to_number<uint32_t>();
        if (session_id == 0) {
            return tl::unexpected("Session ID must be valid");
        }

        const auto nmos_source_uuid = boost::uuids::string_generator()(json.at("nmos_source_uuid").as_string().c_str());
        const auto nmos_flow_uuid = boost::uuids::string_generator()(json.at("nmos_flow_uuid").as_string().c_str());
        const auto nmos_sender_uuid = boost::uuids::string_generator()(json.at("nmos_sender_uuid").as_string().c_str());

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

    RAV_TRACE("SDP:\n{}", rav::sdp::to_string(*sdp, "\n").value_or("n/a"));

    auto response = rtsp::Response(200, "OK", rav::sdp::to_string(*sdp).value_or("n/a"));
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
    update_state(false, !rtsp_path_by_name_.empty(), true);
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

    const auto interface_address_string =
        network_interface_config_.get_interface_ipv4_address(Rank::primary().value()).to_string();

    rtsp::Request request;
    request.method = "ANNOUNCE";
    request.rtsp_headers.set("content-type", "application/sdp");
    request.data = sdp::to_string(*sdp).value_or("n/a");
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

    const auto sdp_format = sdp::make_audio_format(configuration_.audio_format);
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
        network_interface_config_.get_interface_ipv4_address(Rank::primary().value()).to_string(),
    };

    sdp::SessionDescription sdp;
    sdp.origin = origin;
    sdp.session_name = configuration_.session_name;
    sdp.ravenna_clock_domain = clock_domain;
    sdp.reference_clock = ref_clock;
    sdp.media_clock = media_clk;

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

        auto addr = network_interface_config_.get_interface_ipv4_address(dst.interface_by_rank.value());
        if (addr.is_unspecified()) {
            return tl::unexpected(fmt::format("No interface address for rank {}", dst.interface_by_rank.value()));
        }

        RAV_ASSERT(!addr.is_multicast(), "Interface address must not be multicast");

        // Source filter
        sdp::SourceFilter filter {
            sdp::FilterMode::include, sdp::NetwType::internet, sdp::AddrType::ipv4, dst_address_str, {addr.to_string()}
        };

        sdp::MediaDescription media;
        media.connection_infos.push_back(connection_info);
        media.media_type = "audio";
        media.port = 5004;
        media.protocol = "RTP/AVP";
        media.add_or_update_format(sdp_format.value());
        media.add_or_update_source_filter(filter);
        media.ravenna_clock_domain = clock_domain;
        media.ravenna_sync_time = 0;
        media.reference_clock = ref_clock;
        media.media_direction = sdp::MediaDirection::recvonly;
        media.ptime = get_signaled_ptime();
        media.ravenna_framecount = get_framecount();

        if (num_active_destinations > 1) {
            media.mid = dst.interface_by_rank.to_ordinal_latin();
            group.tags.emplace_back(dst.interface_by_rank.to_ordinal_latin());
        }

        sdp.media_descriptions.push_back(std::move(media));
    }

    if (!group.tags.empty()) {
        sdp.group = group;
    }

    return sdp;
}

void rav::RavennaSender::generate_auto_addresses_if_needed() {
    if (generate_auto_addresses_if_needed(configuration_.destinations)) {
        for (auto* subscriber : subscribers_) {
            subscriber->ravenna_sender_configuration_updated(id_, configuration_);
        }
    }
}

bool rav::RavennaSender::generate_auto_addresses_if_needed(std::vector<Destination>& destinations) const {
    bool changed = false;
    for (auto& dst : destinations) {
        if (dst.endpoint.address().is_unspecified()) {
            const auto iface = network_interface_config_.get_interface_for_rank(dst.interface_by_rank.value());
            if (iface != nullptr) {
                auto addr = network_interface_config_.get_interface_ipv4_address(dst.interface_by_rank.value());
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
    return changed;
}

void rav::RavennaSender::update_state(const bool update_advertisement, const bool announce, const bool update_nmos) {
    generate_auto_addresses_if_needed();

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

        nmos_flow_.label = configuration_.session_name;
        nmos_source_.label = configuration_.session_name;
        nmos_sender_.label = configuration_.session_name;
        nmos_sender_.transport = "urn:x-nmos:transport:rtp.mcast";
        nmos_sender_.subscription.active = configuration_.enabled;
        nmos_sender_.interface_bindings.clear();
        for (const auto& dst : configuration_.destinations) {
            if (dst.enabled) {
                const auto iface = network_interface_config_.get_interface_for_rank(dst.interface_by_rank.value());
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
            if (!nmos_node_->add_or_update_source(&nmos_source_)) {
                RAV_ERROR("Failed to add NMOS source with ID: {}", boost::uuids::to_string(nmos_source_.id));
            }
            if (!nmos_node_->add_or_update_flow(&nmos_flow_)) {
                RAV_ERROR("Failed to add NMOS flow with ID: {}", boost::uuids::to_string(nmos_flow_.id));
            }
            if (!nmos_node_->add_or_update_sender(&nmos_sender_)) {
                RAV_ERROR("Failed to add NMOS sender with ID: {}", boost::uuids::to_string(nmos_sender_.id));
            }
            if (auto sdp = build_sdp()) {
                nmos_node_->set_sender_transport_file(&nmos_sender_, std::move(*sdp));
            } else {
                nmos_node_->set_sender_transport_file(&nmos_sender_, {});
            }
        }
    }

    for (const auto& subscriber : subscribers_) {
        subscriber->ravenna_sender_configuration_updated(id_, configuration_);
    }

    if (!configuration_.enabled) {
        std::ignore = rtp_audio_sender_.remove_writer(id_);
        return;  // Done here
    }

    rtp::AudioSender::WriterParameters params;
    params.audio_format = configuration_.audio_format;
    params.packet_time_frames = configuration_.packet_time.framecount(configuration_.audio_format.sample_rate);
    params.payload_type = configuration_.payload_type;
    for (auto& dst : configuration_.destinations) {
        if (dst.enabled && dst.interface_by_rank.value() < params.destinations.size()) {
            params.destinations[dst.interface_by_rank.value()] = dst.endpoint;
        }
    }
    const auto interfaces =
        network_interface_config_.get_array_of_interface_addresses<rtp::AudioSender::k_max_num_redundant_sessions>();
    if (!rtp_audio_sender_.add_writer(id_, params, interfaces)) {
        RAV_ERROR("Failed to add writer");
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

rav::RavennaSender::Destination
rav::tag_invoke(const boost::json::value_to_tag<RavennaSender::Destination>&, const boost::json::value& jv) {
    RavennaSender::Destination dst;
    dst.enabled = jv.at("enabled").as_bool();
    dst.interface_by_rank = Rank(jv.at("interface_by_rank").to_number<uint8_t>());
    const boost::asio::ip::udp::endpoint endpoint(
        boost::asio::ip::make_address(jv.at("address").as_string()), jv.at("port").to_number<uint16_t>()
    );
    dst.endpoint = endpoint;
    return dst;
}

rav::RavennaSender::Configuration
rav::tag_invoke(const boost::json::value_to_tag<RavennaSender::Configuration>&, const boost::json::value& jv) {
    RavennaSender::Configuration config;
    config.session_name = jv.at("session_name").as_string();
    config.destinations = boost::json::value_to<std::vector<RavennaSender::Destination>>(jv.at("destinations"));
    config.ttl = jv.at("ttl").to_number<int32_t>();
    config.payload_type = jv.at("payload_type").to_number<uint8_t>();
    config.packet_time = boost::json::value_to<aes67::PacketTime>(jv.at("packet_time"));
    config.audio_format = boost::json::value_to<AudioFormat>(jv.at("audio_format"));
    config.enabled = jv.at("enabled").as_bool();

    return config;
}
