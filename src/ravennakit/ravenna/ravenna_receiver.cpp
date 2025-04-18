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

tl::expected<rav::rtp::AudioReceiver::StreamParameters, std::string>
find_primary_stream_parameters(const rav::sdp::SessionDescription& sdp) {
    std::optional<rav::AudioFormat> selected_audio_format;
    const rav::sdp::MediaDescription* selected_media_description = nullptr;
    const rav::sdp::ConnectionInfoField* selected_connection_info = nullptr;

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
        break;  // We only need the first media description
    }

    if (!selected_media_description) {
        return tl::unexpected("No suitable media description found");
    }

    if (!selected_audio_format) {
        return tl::unexpected("No media description with supported audio format available");
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
    const auto ptime = selected_media_description->ptime();
    if (ptime.has_value()) {
        packet_time_frames = rav::aes67::PacketTime::framecount(*ptime, selected_audio_format->sample_rate);
    }

    if (packet_time_frames == 0) {
        RAV_WARNING("No ptime attribute found, falling back to framecount");
        const auto framecount = selected_media_description->framecount();
        if (!framecount.has_value()) {
            return tl::unexpected("No framecount attribute found");
        }
        packet_time_frames = *framecount;
    }

    RAV_ASSERT(packet_time_frames > 0, "packet_time_frames must be greater than 0");

    rav::rtp::Session session;

    asio::error_code ec;
    session.rtp_port = selected_media_description->port();
    session.rtcp_port = session.rtp_port + 1;
    session.connection_address = asio::ip::make_address(selected_connection_info->address, ec);
    if (ec) {
        return tl::unexpected(fmt::format("Failed to parse connection address: {}", ec.message()));
    }

    rav::rtp::Filter filter(session.connection_address);

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

    rav::rtp::AudioReceiver::StreamParameters stream_parameters;
    stream_parameters.session = session;
    stream_parameters.filter = filter;
    stream_parameters.audio_format = *selected_audio_format;
    stream_parameters.packet_time_frames = packet_time_frames;

    return stream_parameters;
}

}  // namespace

nlohmann::json rav::RavennaReceiver::to_json() const {
    nlohmann::json root;
    root["configuration"] = configuration_.to_json();
    return root;
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

rav::rtp::AudioReceiver::StreamStats rav::RavennaReceiver::get_stream_stats() const {
    return rtp_audio_receiver_.get_stream_stats();
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
    asio::io_context& io_context, RavennaRtspClient& rtsp_client, rtp::Receiver& rtp_receiver, const Id id,
    ConfigurationUpdate initial_config
) :
    rtsp_client_(rtsp_client), rtp_audio_receiver_(io_context, rtp_receiver), id_(id) {
    if (!initial_config.delay_frames) {
        initial_config.delay_frames = 480;  // 10ms at 48KHz
    }

    auto result = set_configuration(initial_config);
    if (!result) {
        RAV_ERROR("Failed to update configuration: {}", result.error());
    }
}

rav::RavennaReceiver::~RavennaReceiver() {
    std::ignore = rtsp_client_.unsubscribe_from_all_sessions(this);
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

void rav::RavennaReceiver::update_sdp(const sdp::SessionDescription& sdp) {
    auto primary = find_primary_stream_parameters(sdp);

    if (!primary) {
        RAV_ERROR("Failed to find primary stream parameters: {}", primary.error());
        return;
    }

    if (rtp_audio_receiver_.set_parameters(*primary)) {
        for (auto* subscriber : subscribers_) {
            subscriber->ravenna_receiver_stream_updated(rtp_audio_receiver_.get_parameters());
        }
    }
}

rav::Id rav::RavennaReceiver::get_id() const {
    return id_;
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
        subscriber->ravenna_receiver_stream_updated(rtp_audio_receiver_.get_parameters());
        return true;
    }
    return false;
}

bool rav::RavennaReceiver::unsubscribe(const Subscriber* subscriber) {
    return subscribers_.remove(subscriber);
}

std::optional<rav::sdp::SessionDescription> rav::RavennaReceiver::get_sdp() const {
    return rtsp_client_.get_sdp_for_session(configuration_.session_name);
}

std::optional<std::string> rav::RavennaReceiver::get_sdp_text() const {
    return rtsp_client_.get_sdp_text_for_session(configuration_.session_name);
}
