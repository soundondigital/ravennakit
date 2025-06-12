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

const char* audio_format_to_nmos_media_type(const rav::AudioFormat& format) {
    switch (format.encoding) {
        case rav::AudioEncoding::undefined:
            return "audio/undefined";
        case rav::AudioEncoding::pcm_s8:
            return "audio/L8";
        case rav::AudioEncoding::pcm_u8:
            return "audio/U8";  // Non-standard
        case rav::AudioEncoding::pcm_s16:
            return "audio/L16";
        case rav::AudioEncoding::pcm_s24:
            return "audio/L24";
        case rav::AudioEncoding::pcm_s32:
            return "audio/L32";
        case rav::AudioEncoding::pcm_f32:
            return "audio/F32";
        case rav::AudioEncoding::pcm_f64:
            return "audio/F64";
    }
}

}  // namespace

nlohmann::json rav::RavennaReceiver::to_json() const {
    nlohmann::json root;
    root["configuration"] = configuration_.to_json();
    root["uuid"] = boost::uuids::to_string(uuid_);
    return root;
}

tl::expected<void, std::string> rav::RavennaReceiver::restore_from_json(const nlohmann::json& json) {
    try {
        auto config = ConfigurationUpdate::from_json(json.at("configuration"));
        if (!config) {
            return tl::unexpected(config.error());
        }

        const auto uuid_str = json.at("uuid").get<std::string>();
        uuid_ = boost::uuids::string_generator()(uuid_str);

        auto result = set_configuration(*config);
        if (!result) {
            return tl::unexpected(result.error());
        }

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
    boost::asio::io_context& io_context, RavennaRtspClient& rtsp_client, rtp::Receiver& rtp_receiver, const Id id,
    ConfigurationUpdate initial_config
) :
    rtsp_client_(rtsp_client), rtp_audio_receiver_(io_context, rtp_receiver), id_(id) {
    if (!initial_config.delay_frames) {
        initial_config.delay_frames = 480;  // 10ms at 48KHz
    }

    nmos_receiver_.id = boost::uuids::random_generator()();
    nmos_receiver_.caps.media_types.push_back(
        audio_format_to_nmos_media_type(rtp_audio_receiver_.get_parameters().audio_format)
    );

    auto result = set_configuration(initial_config);
    if (!result) {
        RAV_ERROR("Failed to update configuration: {}", result.error());
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
        update_sdp(event.sdp);
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
    for (auto format : media_description.formats()) {
        if (format.to_audio_format() == audio_format) {
            audio_format_found = true;
            break;
        }
    }

    if (!audio_format_found) {
        return tl::unexpected("Audio format not found in media description");
    }

    const rav::sdp::ConnectionInfoField* selected_connection_info = nullptr;

    for (auto& conn : media_description.connection_infos()) {
        if (!is_connection_info_valid(conn)) {
            continue;
        }
        selected_connection_info = &conn;
    }

    if (selected_connection_info == nullptr) {
        // Find connection info in the session description session level
        if (const auto& conn = sdp.connection_info()) {
            if (is_connection_info_valid(*conn)) {
                selected_connection_info = &*conn;
            }
        }
    }

    if (selected_connection_info == nullptr) {
        return tl::unexpected("No suitable connection info found");
    }

    uint16_t packet_time_frames = 0;
    const auto ptime = media_description.ptime();
    if (ptime.has_value()) {
        packet_time_frames = rav::aes67::PacketTime::framecount(*ptime, audio_format.sample_rate);
    }

    if (packet_time_frames == 0) {
        RAV_WARNING("No ptime attribute found, falling back to framecount");
        const auto framecount = media_description.framecount();
        if (!framecount.has_value()) {
            return tl::unexpected("No framecount attribute found");
        }
        packet_time_frames = *framecount;
    }

    RAV_ASSERT(packet_time_frames > 0, "packet_time_frames must be greater than 0");

    rav::rtp::Session session;

    boost::system::error_code ec;
    session.rtp_port = media_description.port();
    session.rtcp_port = session.rtp_port + 1;
    session.connection_address = boost::asio::ip::make_address(selected_connection_info->address, ec);
    if (ec) {
        return tl::unexpected(fmt::format("Failed to parse connection address: {}", ec.message()));
    }

    rav::rtp::Filter filter(session.connection_address);

    const auto& source_filters = media_description.source_filters();
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

    rav::rtp::AudioReceiver::Stream stream;
    stream.session = session;
    stream.filter = filter;
    stream.packet_time_frames = packet_time_frames;
    return stream;
}

const rav::sdp::MediaDescription*
find_media_description_by_mid(const rav::sdp::SessionDescription& sdp, const std::string& mid) {
    for (const auto& media_description : sdp.media_descriptions()) {
        if (media_description.get_mid() == mid) {
            return &media_description;
        }
    }
    return nullptr;
}

}  // namespace

tl::expected<rav::rtp::AudioReceiver::Parameters, std::string>
rav::RavennaReceiver::create_audio_receiver_parameters(const sdp::SessionDescription& sdp) {
    rtp::AudioReceiver::Parameters parameters;

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
        std::optional<AudioFormat> selected_audio_format;

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

        auto stream = create_stream_from_media_description(media_description, sdp, *selected_audio_format);

        if (!stream) {
            RAV_WARNING("Failed to create stream from media description: {}", stream.error());
            continue;
        }

        auto rank = Rank(0);
        stream->rank = rank++;

        parameters.audio_format = *selected_audio_format;
        parameters.streams.push_back(*stream);

        if (auto mid = media_description.get_mid()) {
            auto group = sdp.get_group();
            if (!group) {
                RAV_WARNING("No group found for mid '{}'", *mid);
                break;  // No group found, treating the found stream as the primary
            }

            if (group->get_type() != sdp::Group::Type::dup) {
                RAV_WARNING("Unsupported group type: {}", static_cast<int>(group->get_type()));
                break;  // Unsupported group type
            }

            auto tags = group->get_tags();
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

void rav::RavennaReceiver::update_sdp(const sdp::SessionDescription& sdp) {
    auto parameters = create_audio_receiver_parameters(sdp);

    if (!parameters) {
        RAV_ERROR("Failed to find primary stream parameters: {}", parameters.error());
        return;
    }

    if (rtp_audio_receiver_.set_parameters(*parameters)) {
        RAV_ASSERT(nmos_receiver_.caps.media_types.size() == 1, "Expected exactly one media type");
        nmos_receiver_.label = sdp.session_name();
        nmos_receiver_.caps.media_types.front() = audio_format_to_nmos_media_type(parameters->audio_format);
        if (nmos_node_ != nullptr) {
            if (!nmos_node_->add_or_update_receiver({nmos_receiver_})) {
                RAV_ERROR("Failed to update NMOS receiver with ID: {}", boost::uuids::to_string(nmos_receiver_.id));
            }
        }
        for (auto* subscriber : subscribers_) {
            subscriber->ravenna_receiver_parameters_updated(*parameters);
        }
    }
}

rav::Id rav::RavennaReceiver::get_id() const {
    return id_;
}

const boost::uuids::uuid& rav::RavennaReceiver::get_uuid() const {
    return uuid_;
}

tl::expected<void, std::string> rav::RavennaReceiver::set_configuration(const ConfigurationUpdate& update) {
    // Session name
    if (update.session_name.has_value()) {
        if (update.session_name->empty()) {
            return tl::unexpected("Session name must be valid");
        }
    }

    if (update.delay_frames.has_value() && update.delay_frames != configuration_.delay_frames) {
        configuration_.delay_frames = *update.delay_frames;
        rtp_audio_receiver_.set_delay_frames(configuration_.delay_frames);
    }

    if (update.enabled.has_value() && update.enabled != configuration_.enabled) {
        configuration_.enabled = *update.enabled;
        rtp_audio_receiver_.set_enabled(configuration_.enabled);
    }

    if (update.session_name.has_value() && update.session_name != configuration_.session_name) {
        configuration_.session_name = *update.session_name;

        std::ignore = rtsp_client_.unsubscribe_from_all_sessions(this);

        RAV_ASSERT(!configuration_.session_name.empty(), "Session name must not be empty");

        if (!rtsp_client_.subscribe_to_session(this, configuration_.session_name)) {
            RAV_ERROR("Failed to subscribe to session '{}'", configuration_.session_name);
            return tl::unexpected("Failed to subscribe to session");
        }

        // A restart will happen when the SDP is received
    }

    for (auto* subscriber : subscribers_) {
        subscriber->ravenna_receiver_configuration_updated(*this, configuration_);
    }

    return {};
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
        RAV_ASSERT(!nmos_node_->get_devices().empty(), "NMOS node must have at least one device");
        RAV_ASSERT(nmos_receiver_.is_valid(), "NMOS receiver must be valid at this point");
        if (!nmos_node_->add_or_update_receiver({nmos_receiver_})) {
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

void rav::RavennaReceiver::set_interfaces(const std::map<Rank, boost::asio::ip::address_v4>& interface_addresses) {
    rtp_audio_receiver_.set_interfaces(interface_addresses);
}
