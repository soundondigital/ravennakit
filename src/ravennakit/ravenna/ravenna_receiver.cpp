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
#include "ravennakit/nmos/detail/nmos_media_types.hpp"
#include "ravennakit/rtp/detail/rtp_filter.hpp"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

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

boost::json::object rav::RavennaReceiver::to_boost_json() const {
    return {
        {"configuration", boost::json::value_from(configuration_)},
        {"nmos_receiver_uuid", boost::uuids::to_string(nmos_receiver_.id)}
    };
}

tl::expected<void, std::string> rav::RavennaReceiver::restore_from_json(const boost::json::value& json) {
    try {
        auto config = boost::json::try_value_to<Configuration>(json.at("configuration"));

        if (config.has_error()) {
            return tl::unexpected(config.error().message());
        }

        const auto nmos_receiver_uuid =
            boost::uuids::string_generator()(json.at("nmos_receiver_uuid").as_string().c_str());

        auto result = set_configuration(*config);
        if (!result) {
            return tl::unexpected(result.error());
        }

        nmos_receiver_.id = nmos_receiver_uuid;

        return {};
    } catch (const std::exception& e) {
        return tl::unexpected(fmt::format("Failed to restore RavennaReceiver from JSON: {}", e.what()));
    }
}

std::optional<uint32_t> rav::RavennaReceiver::read_data_realtime(
    uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp
) {
    return rtp_audio_receiver_.read_data_realtime(buffer, buffer_size, at_timestamp);
}

std::optional<uint32_t> rav::RavennaReceiver::read_audio_data_realtime(
    const AudioBufferView<float>& output_buffer, const std::optional<uint32_t> at_timestamp
) {
    return rtp_audio_receiver_.read_audio_data_realtime(output_buffer, at_timestamp);
}

rav::rtp::AudioReceiver::SessionStats rav::RavennaReceiver::get_stream_stats(const Rank rank) const {
    return rtp_audio_receiver_.get_session_stats(rank);
}

const rav::nmos::ReceiverAudio& rav::RavennaReceiver::get_nmos_receiver() const {
    return nmos_receiver_;
}

rav::RavennaReceiver::RavennaReceiver(
    boost::asio::io_context& io_context, RavennaRtspClient& rtsp_client, rtp::Receiver& rtp_receiver, const Id id
) :
    rtsp_client_(rtsp_client), rtp_audio_receiver_(io_context, rtp_receiver), id_(id) {
    nmos_receiver_.id = boost::uuids::random_generator()();

    for (auto& encoding : k_supported_encodings) {
        nmos_receiver_.caps.media_types.emplace_back(nmos::audio_encoding_to_nmos_media_type(encoding));
    }

    rtp_audio_receiver_.on_data_received([this](const WrappingUint32 packet_timestamp) {
        for (auto* subscriber : subscribers_) {
            subscriber->on_data_received(packet_timestamp);
        }
    });

    rtp_audio_receiver_.on_data_ready([this](const WrappingUint32 packet_timestamp) {
        for (auto* subscriber : subscribers_) {
            subscriber->on_data_ready(packet_timestamp);
        }
    });

    rtp_audio_receiver_.on_state_changed(
        [this](const rtp::AudioReceiver::Stream& session, const rtp::AudioReceiver::State state) {
            for (auto* subscriber : subscribers_) {
                subscriber->ravenna_receiver_stream_state_updated(session, state);
            }
        }
    );
}

rav::RavennaReceiver::~RavennaReceiver() {
    std::ignore = rtsp_client_.unsubscribe_from_all_sessions(this);
    if (nmos_node_ != nullptr) {
        if (!nmos_node_->remove_receiver(nmos_receiver_.id)) {
            RAV_ERROR("Failed to remove NMOS receiver with ID: {}", boost::uuids::to_string(nmos_receiver_.id));
        }
    }
}

void rav::RavennaReceiver::on_announced(const RavennaRtspClient::AnnouncedEvent& event) {
    try {
        RAV_ASSERT(event.session_name == configuration_.session_name, "Expecting session_name to match");
        handle_announced_sdp(event.sdp);
        RAV_TRACE("SDP updated for session '{}'", configuration_.session_name);
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to process SDP for session '{}': {}", configuration_.session_name, e.what());
    }
}

namespace {

tl::expected<rav::rtp::AudioReceiver::Stream, std::string> create_stream_from_media_description(
    const rav::sdp::MediaDescription& media_description, const rav::sdp::SessionDescription& sdp,
    const rav::AudioFormat& audio_format
) {
    bool audio_format_found = false;
    for (const auto& format : media_description.formats) {
        if (rav::sdp::make_audio_format(format) == audio_format) {
            audio_format_found = true;
            break;
        }
    }

    if (!audio_format_found) {
        return tl::unexpected("Audio format not found in media description");
    }

    const rav::sdp::ConnectionInfoField* selected_connection_info = nullptr;

    for (auto& conn : media_description.connection_infos) {
        if (!is_connection_info_valid(conn)) {
            continue;
        }
        selected_connection_info = &conn;
    }

    if (selected_connection_info == nullptr) {
        // Find connection info in the session description session level
        if (const auto& conn = sdp.connection_info) {
            if (is_connection_info_valid(*conn)) {
                selected_connection_info = &*conn;
            }
        }
    }

    if (selected_connection_info == nullptr) {
        return tl::unexpected("No suitable connection info found");
    }

    uint16_t packet_time_frames = 0;
    const auto ptime = media_description.ptime;
    if (ptime.has_value()) {
        packet_time_frames = rav::aes67::PacketTime::framecount(*ptime, audio_format.sample_rate);
    }

    if (packet_time_frames == 0) {
        RAV_WARNING("No ptime attribute found, falling back to framecount");
        const auto framecount = media_description.ravenna_framecount;
        if (!framecount.has_value()) {
            return tl::unexpected("No framecount attribute found");
        }
        packet_time_frames = *framecount;
    }

    RAV_ASSERT(packet_time_frames > 0, "packet_time_frames must be greater than 0");

    rav::rtp::Session session;

    boost::system::error_code ec;
    session.rtp_port = media_description.port;
    session.rtcp_port = session.rtp_port + 1;
    session.connection_address = boost::asio::ip::make_address(selected_connection_info->address, ec);
    if (ec) {
        return tl::unexpected(fmt::format("Failed to parse connection address: {}", ec.message()));
    }

    rav::rtp::Filter filter(session.connection_address);

    const auto& source_filters = media_description.source_filters;
    if (!source_filters.empty()) {
        if (filter.add_filters(source_filters) == 0) {
            RAV_WARNING("No suitable source filters found in SDP");
        }
    } else {
        const auto& sdp_source_filters = sdp.source_filters;
        if (!sdp_source_filters.empty()) {
            if (filter.add_filters(sdp_source_filters) == 0) {
                RAV_WARNING("No suitable source filters found in SDP");
            }
        }
    }

    rav::rtp::AudioReceiver::Stream stream;
    stream.session = session;
    stream.filter = filter;
    stream.packet_time_frames = packet_time_frames;
    return stream;
}

const rav::sdp::MediaDescription*
find_media_description_by_mid(const rav::sdp::SessionDescription& sdp, const std::string& mid) {
    for (const auto& media_description : sdp.media_descriptions) {
        if (media_description.mid == mid) {
            return &media_description;
        }
    }
    return nullptr;
}

}  // namespace

tl::expected<rav::rtp::AudioReceiver::Parameters, std::string>
rav::RavennaReceiver::create_audio_receiver_parameters(const sdp::SessionDescription& sdp) {
    rtp::AudioReceiver::Parameters parameters;

    for (auto& media_description : sdp.media_descriptions) {
        if (media_description.media_type != "audio") {
            RAV_WARNING("Unsupported media type: {}", media_description.media_type);
            continue;
        }

        if (media_description.protocol != "RTP/AVP") {
            RAV_WARNING("Unsupported protocol {}", media_description.protocol);
            continue;
        }

        // The first acceptable payload format from the beginning of the list SHOULD be used for the session.
        // https://datatracker.ietf.org/doc/html/rfc8866#name-media-descriptions-m
        std::optional<AudioFormat> selected_audio_format;

        for (auto& format : media_description.formats) {
            selected_audio_format = rav::sdp::make_audio_format(format);
            if (!selected_audio_format) {
                RAV_WARNING("Not a supported audio format: {}", rav::sdp::to_string(format));
                continue;
            }
            break;
        }

        if (!selected_audio_format) {
            RAV_WARNING("No supported audio format found");
            continue;
        }

        auto stream = create_stream_from_media_description(media_description, sdp, *selected_audio_format);

        if (!stream) {
            RAV_WARNING("Failed to create stream from media description: {}", stream.error());
            continue;
        }

        auto rank = Rank(0);
        stream->rank = rank++;

        parameters.audio_format = *selected_audio_format;
        parameters.streams.push_back(*stream);

        if (auto mid = media_description.mid) {
            const auto group = sdp.group;
            if (!group) {
                RAV_WARNING("No group found for mid '{}'", *mid);
                break;  // No group found, treating the found stream as the primary
            }

            if (group->type != sdp::Group::Type::dup) {
                RAV_WARNING("Unsupported group type: {}", static_cast<int>(group->type));
                break;  // Unsupported group type
            }

            auto tags = group->tags;
            if (std::find(tags.begin(), tags.end(), *mid) == tags.end()) {
                RAV_WARNING("Mid '{}' not found in group tags", *mid);
                break;  // Mid not found in group tags
            }

            tags.erase(std::remove(tags.begin(), tags.end(), *mid), tags.end());

            for (auto& tag : tags) {
                auto* media_desc = find_media_description_by_mid(sdp, tag);
                if (!media_desc) {
                    RAV_WARNING("Media description with mid '{}' not found", tag);
                    continue;
                }
                auto dup_stream = create_stream_from_media_description(*media_desc, sdp, *selected_audio_format);
                if (!dup_stream) {
                    RAV_WARNING("Failed to create stream from media description: {}", dup_stream.error());
                    continue;
                }
                dup_stream->rank = rank++;
                parameters.streams.push_back(*dup_stream);
            }
        }

        return parameters;
    }

    return tl::unexpected("No suitable media description found");
}

void rav::RavennaReceiver::handle_announced_sdp(const sdp::SessionDescription& sdp) {
    if (!configuration_.auto_update_sdp) {
        RAV_ERROR("auto_update_sdp is false, not expecting to receive SDP updates");
        return;
    }

    auto config = configuration_;
    config.sdp = sdp;
    if (!set_configuration(config)) {
        RAV_ERROR("Failed to set configuration from announced SDP");
    }
}

tl::expected<void, std::string> rav::RavennaReceiver::update_state(const bool update_rtsp, bool update_nmos) {
    auto parameters = create_audio_receiver_parameters(configuration_.sdp);

    if (!configuration_.auto_update_sdp) {
        configuration_.session_name = configuration_.sdp.session_name;
    }

    rtp_audio_receiver_.set_delay_frames(configuration_.delay_frames);
    rtp_audio_receiver_.set_enabled(parameters.has_value() && configuration_.enabled);

    if (parameters.has_value() && rtp_audio_receiver_.set_parameters(*parameters)) {
        update_nmos = true;
        for (auto* subscriber : subscribers_) {
            subscriber->ravenna_receiver_parameters_updated(*parameters);
        }
    }

    if (update_rtsp) {
        std::ignore = rtsp_client_.unsubscribe_from_all_sessions(this);

        if (configuration_.enabled && configuration_.auto_update_sdp) {
            if (!rtsp_client_.subscribe_to_session(this, configuration_.session_name)) {
                RAV_ERROR("Failed to subscribe to session '{}'", configuration_.session_name);
                return tl::unexpected("Failed to subscribe to session");
            }
        }
    }

    for (auto* subscriber : subscribers_) {
        subscriber->ravenna_receiver_configuration_updated(*this, configuration_);
    }

    if (update_nmos) {
        nmos_receiver_.label = configuration_.session_name;
        nmos_receiver_.subscription.active = configuration_.enabled;
        nmos_receiver_.transport = "urn:x-nmos:transport:rtp.mcast";
        nmos_receiver_.interface_bindings.clear();
        for (const auto& [rank, id] : network_interface_config_.interfaces) {
            nmos_receiver_.interface_bindings.push_back(id);
        }

        if (nmos_node_ != nullptr) {
            RAV_ASSERT(nmos_receiver_.is_valid(), "NMOS receiver must be valid at this point");
            if (!nmos_node_->add_or_update_receiver(nmos_receiver_)) {
                RAV_ERROR("Failed to update NMOS receiver with ID: {}", boost::uuids::to_string(nmos_receiver_.id));
                return tl::unexpected("Failed to update NMOS receiver");
            }
        }
    }

    return {};
}

rav::Id rav::RavennaReceiver::get_id() const {
    return id_;
}

const boost::uuids::uuid& rav::RavennaReceiver::get_uuid() const {
    return nmos_receiver_.id;
}

tl::expected<void, std::string> rav::RavennaReceiver::set_configuration(Configuration config) {
    // Validate the configuration

    if (config.auto_update_sdp) {
        if (config.session_name.empty()) {
            return tl::unexpected("Session name must not be empty when auto_update_sdp is true");
        }
    }

    if (config.delay_frames == 0) {
        RAV_WARNING("Delay is set to 0 frames, which is most likely not what you want");
    }

    // Determine changes to apply

    bool update_nmos = false;
    bool update_rtsp = false;

    if (config.enabled != configuration_.enabled) {
        update_nmos = true;
        update_rtsp = true;
    }

    if (config.session_name != configuration_.session_name) {
        update_nmos = true;
        update_rtsp = true;
    }

    if (config.auto_update_sdp != configuration_.auto_update_sdp) {
        update_rtsp = true;
    }

    // Apply the configuration changes

    configuration_ = std::move(config);

    return update_state(update_rtsp, update_nmos);
}

const rav::RavennaReceiver::Configuration& rav::RavennaReceiver::get_configuration() const {
    return configuration_;
}

bool rav::RavennaReceiver::subscribe(Subscriber* subscriber) {
    if (subscribers_.add(subscriber)) {
        subscriber->ravenna_receiver_configuration_updated(*this, configuration_);
        const auto parameters = rtp_audio_receiver_.get_parameters();
        subscriber->ravenna_receiver_parameters_updated(parameters);
        for (auto& stream : parameters.streams) {
            const auto state = rtp_audio_receiver_.get_state_for_stream(stream.session);
            if (!state) {
                RAV_ERROR("Failed to get state for stream {}", stream.session.to_string());
                continue;
            }
            subscriber->ravenna_receiver_stream_state_updated(stream, *state);
        }
        return true;
    }
    return false;
}

bool rav::RavennaReceiver::unsubscribe(const Subscriber* subscriber) {
    return subscribers_.remove(subscriber);
}

void rav::RavennaReceiver::set_nmos_node(nmos::Node* nmos_node) {
    if (nmos_node_ == nmos_node) {
        return;
    }
    nmos_node_ = nmos_node;
    if (nmos_node_ != nullptr) {
        RAV_ASSERT(nmos_receiver_.is_valid(), "NMOS receiver must be valid at this point");
        if (!nmos_node_->add_or_update_receiver(nmos_receiver_)) {
            RAV_ERROR("Failed to add NMOS receiver with ID: {}", boost::uuids::to_string(nmos_receiver_.id));
        }
    }
}

void rav::RavennaReceiver::set_nmos_device_id(const boost::uuids::uuid& device_id) {
    nmos_receiver_.device_id = device_id;
}

std::optional<rav::sdp::SessionDescription> rav::RavennaReceiver::get_sdp() const {
    return rtsp_client_.get_sdp_for_session(configuration_.session_name);
}

std::optional<std::string> rav::RavennaReceiver::get_sdp_text() const {
    return rtsp_client_.get_sdp_text_for_session(configuration_.session_name);
}

void rav::RavennaReceiver::set_network_interface_config(NetworkInterfaceConfig network_interface_config) {
    if (network_interface_config_ == network_interface_config) {
        return;  // No change in network interface configuration
    }
    network_interface_config_ = std::move(network_interface_config);
    rtp_audio_receiver_.set_interfaces(network_interface_config_.get_interface_ipv4_addresses());
    auto result = update_state(false, true);
    if (!result) {
        RAV_ERROR("Failed to update state after setting network interface config: {}", result.error());
    }
}

void rav::tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const RavennaReceiver::Configuration& config
) {
    jv = {
        {"session_name", config.session_name},
        {"delay_frames", config.delay_frames},
        {"enabled", config.enabled},
        {"auto_update_sdp", config.auto_update_sdp},
        {"sdp", rav::sdp::to_string(config.sdp)}
    };
}

rav::RavennaReceiver::Configuration
rav::tag_invoke(const boost::json::value_to_tag<RavennaReceiver::Configuration>&, const boost::json::value& jv) {
    RavennaReceiver::Configuration config;
    config.session_name = jv.at("session_name").as_string();
    config.delay_frames = jv.at("delay_frames").to_number<uint32_t>();
    config.enabled = jv.at("enabled").as_bool();
    config.auto_update_sdp = jv.at("auto_update_sdp").as_bool();
    config.sdp = sdp::parse_session_description(jv.at("sdp").as_string().c_str()).value();
    return config;
}
