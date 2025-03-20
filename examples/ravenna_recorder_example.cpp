/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/file.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/core/audio/formats/wav_audio_format.hpp"
#include "ravennakit/core/streams/file_output_stream.hpp"
#include "ravennakit/dnssd/bonjour/bonjour_browser.hpp"
#include "ravennakit/ravenna/ravenna_rtsp_client.hpp"
#include "ravennakit/ravenna/ravenna_receiver.hpp"

#include <CLI/App.hpp>
#include <asio/io_context.hpp>
#include <utility>

namespace examples {
/**
 * A class that is a subscriber to a rtp_stream_receiver and writes the audio data to a wav file.
 */
class stream_recorder: public rav::rtp::rtp_stream_receiver::subscriber {
  public:
    explicit stream_recorder(std::unique_ptr<rav::ravenna_receiver> sink) : receiver_(std::move(sink)) {
        if (receiver_) {
            if (!receiver_->subscribe(this)) {
                RAV_WARNING("Failed to add subscriber");
            }
        }
    }

    ~stream_recorder() override {
        if (receiver_) {
            if (!receiver_->unsubscribe(this)) {
                RAV_WARNING("Failed to remove subscriber");
            }
        }
        close();
    }

    void close() {
        if (wav_writer_) {
            if (!wav_writer_->finalize()) {
                RAV_ERROR("Failed to finalize wav file");
            }
            wav_writer_.reset();
        }
        if (file_output_stream_) {
            file_output_stream_.reset();
        }
    }

    void rtp_stream_receiver_updated(const rav::rtp::rtp_stream_receiver::stream_updated_event& event) override {
        close();

        if (receiver_ == nullptr) {
            RAV_ERROR("No receiver available");
            return;
        }

        audio_format_ = event.selected_audio_format;
        file_output_stream_ =
            std::make_unique<rav::file_output_stream>(rav::file(receiver_->get_session_name() + ".wav"));
        wav_writer_ = std::make_unique<rav::wav_audio_format::writer>(
            *file_output_stream_, rav::wav_audio_format::format_code::pcm, audio_format_.sample_rate,
            audio_format_.num_channels, audio_format_.bytes_per_sample() * 8
        );
        audio_data_.resize(event.packet_time_frames * audio_format_.bytes_per_frame());
    }

    void on_data_ready(const rav::wrapping_uint32 timestamp) override {
        if (!receiver_->read_data_realtime(audio_data_.data(), audio_data_.size(), timestamp.value())) {
            RAV_ERROR("Failed to read audio data");
            return;
        }
        if (audio_format_.byte_order == rav::audio_format::byte_order::be) {
            rav::byte_order::swap_bytes(audio_data_.data(), audio_data_.size(), audio_format_.bytes_per_sample());
        }
        if (!wav_writer_->write_audio_data(audio_data_.data(), audio_data_.size())) {
            RAV_ERROR("Failed to write audio data");
        }
    }

  private:
    std::unique_ptr<rav::ravenna_receiver> receiver_;
    std::unique_ptr<rav::file_output_stream> file_output_stream_;
    std::unique_ptr<rav::wav_audio_format::writer> wav_writer_;
    std::vector<uint8_t> audio_data_;
    rav::audio_format audio_format_;
    std::optional<rav::wrapping_uint32> stream_ts_;
};

class ravenna_recorder {
  public:
    explicit ravenna_recorder(const std::string& interface_address) {
        rtsp_client_ = std::make_unique<rav::ravenna_rtsp_client>(io_context_, browser_);

        rav::rtp::rtp_receiver::configuration config;
        config.interface_address = asio::ip::make_address(interface_address);
        rtp_receiver_ = std::make_unique<rav::rtp::rtp_receiver>(io_context_, config);
    }

    ~ravenna_recorder() = default;

    void add_stream(const std::string& stream_name) {
        auto receiver = std::make_unique<rav::ravenna_receiver>(*rtsp_client_, *rtp_receiver_);
        receiver->set_delay(480);  // 10ms @ 48kHz
        if (!receiver->subscribe_to_session(stream_name)) {
            RAV_ERROR("Failed to subscribe to session");
            return;
        }
        recorders_.emplace_back(std::make_unique<stream_recorder>(std::move(receiver)));
    }

    void start() {
        io_context_.run();
    }

    void stop() {
        io_context_.stop();
    }

  private:
    asio::io_context io_context_;
    rav::ravenna_browser browser_ {io_context_};
    std::unique_ptr<rav::ravenna_rtsp_client> rtsp_client_;
    std::unique_ptr<rav::rtp::rtp_receiver> rtp_receiver_;
    std::vector<std::unique_ptr<stream_recorder>> recorders_;
};

}  // namespace examples

/**
 * This examples demonstrates how to receive audio streams from a RAVENNA device and write the audio data to wav files.
 * It sets up a RAVENNA sink that listens for announcements from a RAVENNA device and starts receiving audio data.
 * Separate files for each stream are created and existing files will be overwritten.
 */
int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> stream_names;
    app.add_option("stream_names", stream_names, "The names of the streams to receive")->required();

    std::string interface_address = "0.0.0.0";
    app.add_option("--interface-addr", interface_address, "The interface address");

    CLI11_PARSE(app, argc, argv);

    examples::ravenna_recorder recorder_example(interface_address);

    for (auto& stream_name : stream_names) {
        recorder_example.add_stream(stream_name);
    }

    std::thread cin_thread([&recorder_example] {
        fmt::println("Press return key to stop...");
        std::cin.get();
        recorder_example.stop();
    });

    recorder_example.start();
    cin_thread.join();

    return 0;
}
