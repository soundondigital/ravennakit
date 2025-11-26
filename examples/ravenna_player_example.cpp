/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/file.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/core/audio/formats/wav_audio_format.hpp"
#include "ravennakit/core/platform/apple/priority.hpp"
#include "ravennakit/core/streams/file_input_stream.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ravenna/ravenna_node.hpp"
#include "ravennakit/ravenna/ravenna_sender.hpp"
#include "ravennakit/ravenna/ravenna_sender.hpp"

#include <CLI/App.hpp>
#include <boost/asio/io_context.hpp>
#include <utility>

namespace examples {

static constexpr uint32_t k_frames_per_read = 1024;

/**
 * Holds the logic for transmitting a wav file over the network.
 */
class WavFilePlayer {
  public:
    explicit WavFilePlayer(rav::RavennaNode& ravenna_node, const std::filesystem::path& file_to_play, const std::string& session_name) :
        ravenna_node_(ravenna_node) {
        if (!std::filesystem::exists(file_to_play)) {
            throw std::runtime_error("File does not exist: " + file_to_play.string());
        }

        auto file_input_stream = std::make_unique<rav::FileInputStream>(file_to_play);
        auto reader = std::make_unique<rav::WavAudioFormat::Reader>(std::move(file_input_stream));

        const auto format = reader->get_audio_format();
        if (!format) {
            throw std::runtime_error("Failed to read audio format from file: " + file_to_play.string());
        }

        audio_format_ = *format;

        rav::RavennaSender::Configuration config;
        config.session_name = session_name;
        config.audio_format = audio_format_.with_byte_order(rav::AudioFormat::ByteOrder::be);
        config.enabled = true;
        config.packet_time = rav::aes67::PacketTime::ms_1();
        config.payload_type = 98;
        config.ttl = 15;
        config.destinations.emplace_back(rav::RavennaSender::Destination {rav::rank::primary, {{}, 5004}, true});

        auto result = ravenna_node_.create_sender(config).get();
        if (!result) {
            throw std::runtime_error("Failed to create sender: " + result.error());
        }
        id_ = *result;

        reader_ = std::move(reader);
        audio_buffer_.resize(k_frames_per_read * audio_format_.bytes_per_frame());

        ravenna_node_.subscribe_to_ptp_instance(&ptp_subscriber_).wait();
    }

    ~WavFilePlayer() {
        ravenna_node_.unsubscribe_from_ptp_instance(&ptp_subscriber_).wait();
    }

    void send_audio() {
        TRACY_ZONE_SCOPED;

        const auto& clock = ptp_subscriber_.get_local_clock();
        if (!clock.is_calibrated()) {
            return;
        }

        const auto ptp_ts = clock.now().to_rtp_timestamp32(audio_format_.sample_rate);
        // Positive means audio device is ahead of the PTP clock, negative means behind
        const auto drift = rav::WrappingUint32(ptp_ts).diff(rav::WrappingUint32(rtp_ts_));

        if (static_cast<uint32_t>(std::abs(drift)) > k_frames_per_read) {
            rtp_ts_ = ptp_ts;
        }

        TRACY_PLOT("drift", static_cast<int64_t>(drift));

        if (ptp_ts < rtp_ts_) {
            return;  // Not yet time to send
        }

        if (reader_->remaining_audio_data() == 0) {
            reader_->set_read_position(0);
        }

        auto result = reader_->read_audio_data(audio_buffer_.data(), audio_buffer_.size());
        if (!result) {
            RAV_LOG_ERROR("Failed to read audio data: {}", rav::InputStream::to_string(result.error()));
            return;
        }
        const auto num_read = result.value();
        if (num_read == 0) {
            RAV_LOG_ERROR("No bytes read");
        }

        if (audio_format_.byte_order == rav::AudioFormat::ByteOrder::le) {
            // Convert to big endian
            rav::swap_bytes(audio_buffer_.data(), num_read, audio_format_.bytes_per_sample());
        }

        if (!ravenna_node_.send_data_realtime(id_, rav::BufferView(audio_buffer_.data(), num_read).const_view(), rtp_ts_)) {
            RAV_LOG_ERROR("Failed to send audio data");
        }

        rtp_ts_ += static_cast<uint32_t>(num_read) / audio_format_.bytes_per_frame();
    }

  private:
    rav::RavennaNode& ravenna_node_;
    rav::ptp::Instance::Subscriber ptp_subscriber_;
    rav::Id id_;
    rav::AudioFormat audio_format_;
    std::vector<uint8_t> audio_buffer_;
    uint32_t rtp_ts_ {};
    std::unique_ptr<rav::WavAudioFormat::Reader> reader_;
};

}  // namespace examples

/**
 * This examples demonstrates how to create a source and send audio onto the network. It does this by reading audio from
 * a wav file on disk and sending it as multicast audio packets.
 * Note: this examples shows custom implementation of sending streams, the easier, higher level and recommended approach
 * is to use the RavennaNode class (see ravenna_node_example).
 */
int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Player example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> file_paths;
    app.add_option("files", file_paths, "The files to stream")->required();

    std::string interfaces;
    app.add_option("--interfaces", interfaces, R"(The interfaces to use. Examples: "en1,en2" "192.168.1.1,192.168.2.1")");

    CLI11_PARSE(app, argc, argv);

    const auto network_config = rav::parse_network_interface_config_from_string(interfaces);
    if (!network_config) {
        RAV_LOG_ERROR("Failed to parse network interface config");
        return 1;
    }

    rav::RavennaNode ravenna_node;
    ravenna_node.set_network_interface_config(*network_config).wait();

    std::vector<std::unique_ptr<examples::WavFilePlayer>> wav_file_players;

    for (auto& file_path : file_paths) {
        auto file = std::filesystem::path(file_path);
        const auto file_session_name = file.filename().string();

        wav_file_players.emplace_back(
            std::make_unique<examples::WavFilePlayer>(
                ravenna_node, file, file_session_name + " " + std::to_string(wav_file_players.size() + 1)
            )
        );
    }

    std::atomic keep_going = true;
    std::thread audio_thread([&]() mutable {
        TRACY_SET_THREAD_NAME("ravenna_node_network");

        while (keep_going.load(std::memory_order_relaxed)) {
            for (const auto& wav_file_player : wav_file_players) {
                wav_file_player->send_audio();
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    fmt::println("Press return key to stop...");
    std::string line;
    std::getline(std::cin, line);

    keep_going.store(false, std::memory_order_relaxed);
    audio_thread.join();

    return 0;
}
