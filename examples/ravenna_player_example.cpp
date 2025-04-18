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
#include "ravennakit/core/streams/file_input_stream.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ravenna/ravenna_sender.hpp"

#include <CLI/App.hpp>
#include <asio/io_context.hpp>
#include <utility>

namespace examples {

static constexpr uint32_t k_frames_per_read = 1024;

/**
 * Holds the logic for transmitting a wav file over the network.
 */
class wav_file_player: public rav::ptp::Instance::Subscriber {
  public:
    explicit wav_file_player(
        asio::io_context& io_context, rav::dnssd::Advertiser& advertiser, rav::rtsp::Server& rtsp_server,
        rav::ptp::Instance& ptp_instance, rav::Id::Generator& id_generator,
        const asio::ip::address_v4& interface_address, const rav::File& file_to_play, const std::string& session_name
    ) :
        ptp_instance_(ptp_instance), timer_(io_context) {
        if (!file_to_play.exists()) {
            throw std::runtime_error("File does not exist: " + file_to_play.path().string());
        }

        auto id = id_generator.next();
        auto sender = std::make_unique<rav::RavennaSender>(
            io_context, advertiser, rtsp_server, ptp_instance, id, id.value(), interface_address
        );

        auto file_input_stream = std::make_unique<rav::FileInputStream>(file_to_play);
        auto reader = std::make_unique<rav::WavAudioFormat::Reader>(std::move(file_input_stream));

        const auto format = reader->get_audio_format();
        if (!format) {
            throw std::runtime_error("Failed to read audio format from file: " + file_to_play.path().string());
        }

        audio_format_ = *format;

        rav::RavennaSender::ConfigurationUpdate update;
        update.session_name = session_name;
        update.audio_format = audio_format_.with_byte_order(rav::AudioFormat::ByteOrder::be);
        update.enabled = true;
        const auto result = sender->set_configuration(update);
        if (!result) {
            throw std::runtime_error("Failed to update configuration for transmitter: " + result.error());
        }

        reader_ = std::move(reader);
        sender_ = std::move(sender);

        audio_buffer_.resize(k_frames_per_read * audio_format_.bytes_per_frame());

        if (!ptp_instance_.subscribe(this)) {
            RAV_ERROR("Failed to subscribe to PTP instance");
        }
    }

    ~wav_file_player() override {
        if (!ptp_instance_.unsubscribe(this)) {
            RAV_ERROR("Failed to unsubscribe from PTP instance");
        }
    }

    void ptp_port_changed_state(const rav::ptp::Port& port) override {
        if (port.state() == rav::ptp::State::slave && state_ == State::stopped) {
            RAV_INFO("Port state changed to slave, start player");

            auto start_at = get_local_clock().now();
            start_at.add_seconds(0.5);
            rtp_ts_ = static_cast<uint32_t>(start_at.to_samples(audio_format_.sample_rate));
            start_timer();
            state_ = State::playing;
        }
    }

  private:
    enum class State {
        stopped,
        playing,
    };
    rav::ptp::Instance& ptp_instance_;
    State state_ = State::stopped;
    rav::AudioFormat audio_format_;
    std::vector<uint8_t> audio_buffer_;
    uint32_t rtp_ts_ {};
    std::unique_ptr<rav::WavAudioFormat::Reader> reader_;
    std::unique_ptr<rav::RavennaSender> sender_;
    asio::high_resolution_timer timer_;

    void start_timer() {
#if RAV_WINDOWS
        // A dirty hack to get the precision somewhat right. This is only temporary since we have to come up with a much
        // tighter solution anyway.
        auto expires_after = std::chrono::microseconds(1);
#else
        // A tenth of the frames per read time
        auto expires_after = std::chrono::milliseconds(k_frames_per_read / audio_format_.sample_rate * 1000 / 10);
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
            send_audio();
            start_timer();
        });
    }

    void send_audio() {
        TRACY_ZONE_SCOPED;

        const auto clock = get_local_clock();
        const auto ptp_ts = static_cast<uint32_t>(clock.now().to_samples(audio_format_.sample_rate));
        if (ptp_ts < rtp_ts_) {
            return;  // Not yet time to send
        }

        if (!clock.is_calibrated()) {
            return;  // Clock is not calibrated
        }

        if (reader_->remaining_audio_data() == 0) {
            reader_->set_read_position(0);
        }

        auto result = reader_->read_audio_data(audio_buffer_.data(), audio_buffer_.size());
        if (!result) {
            RAV_ERROR("Failed to read audio data: {}", rav::InputStream::to_string(result.error()));
            return;
        }
        const auto num_read = result.value();
        if (num_read == 0) {
            RAV_ERROR("No bytes read");
        }

        const auto drift = rav::WrappingUint32(ptp_ts).diff(rav::WrappingUint32(rtp_ts_));
        std::ignore = drift; // For when Tracy is disabled
        // Positive means audio device is ahead of the PTP clock, negative means behind

        TRACY_PLOT("drift", static_cast<int64_t>(drift));

        if (audio_format_.byte_order == rav::AudioFormat::ByteOrder::le) {
            // Convert to big endian
            rav::swap_bytes(audio_buffer_.data(), num_read, audio_format_.bytes_per_sample());
        }

        if (!sender_->send_data_realtime(rav::BufferView(audio_buffer_.data(), num_read).const_view(), rtp_ts_)) {
            RAV_ERROR("Failed to send audio data");
        }

        rtp_ts_ += static_cast<uint32_t>(num_read) / audio_format_.bytes_per_frame();
    }
};

}  // namespace examples

/**
 * This examples demonstrates how to create a source and send audio onto the network. It does this by reading audio from
 * a wav file on disk and sending it as multicast audio packets.
 */
int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Player example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> file_paths;
    app.add_option("files", file_paths, "The files to stream")->required();

    std::string interface_address_string = "0.0.0.0";
    app.add_option("--interface-addr", interface_address_string, "The interface address");

    CLI11_PARSE(app, argc, argv);

    const auto interface_address = asio::ip::make_address_v4(interface_address_string);

    asio::io_context io_context;

    std::vector<std::unique_ptr<examples::wav_file_player>> wav_file_players;

    auto advertiser = rav::dnssd::Advertiser::create(io_context);
    rav::rtsp::Server rtsp_server(io_context, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 5005));

    // PTP
    rav::ptp::Instance ptp_instance(io_context);

    if (const auto result = ptp_instance.add_port(interface_address); !result) {
        RAV_THROW_EXCEPTION("Failed to add PTP port: {}", to_string(result.error()));
    }

    // ID generator
    rav::Id::Generator id_generator;

    for (auto& file_path : file_paths) {
        auto file = rav::File(file_path);
        const auto file_session_name = file.path().filename().string();

        wav_file_players.emplace_back(
            std::make_unique<examples::wav_file_player>(
                io_context, *advertiser, rtsp_server, ptp_instance, id_generator, interface_address, file,
                file_session_name + " " + std::to_string(wav_file_players.size() + 1)
            )
        );
    }

    while (!io_context.stopped()) {
        io_context.poll();
    }

    return 0;
}
