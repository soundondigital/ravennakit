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
#include <boost/asio/io_context.hpp>
#include <utility>

namespace examples {

/**
 * A class that is a subscriber to a rtp_stream_receiver and writes the audio data to a wav file.
 */
class StreamRecorder: public rav::RavennaReceiver::Subscriber {
  public:
    explicit StreamRecorder(std::unique_ptr<rav::RavennaReceiver> receiver) : receiver_(std::move(receiver)) {
        if (receiver_) {
            if (!receiver_->subscribe(this)) {
                RAV_WARNING("Failed to add subscriber");
            }
        }
    }

    ~StreamRecorder() override {
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

    void ravenna_receiver_parameters_updated(const rav::rtp::Receiver3::ReaderParameters& parameters) override {
        if (parameters.streams.empty()) {
            RAV_WARNING("No streams available");
            return;
        }

        if (!parameters.audio_format.is_valid()) {
            return;
        }

        close();

        if (receiver_ == nullptr) {
            RAV_ERROR("No receiver available");
            return;
        }

        auto file_path = rav::File(receiver_->get_configuration().session_name + ".wav").absolute();

        RAV_INFO(
            "Recording stream: {} to file: {}", receiver_->get_configuration().session_name, file_path.to_string()
        );

        audio_format_ = parameters.audio_format;
        file_output_stream_ = std::make_unique<rav::FileOutputStream>(file_path);
        wav_writer_ = std::make_unique<rav::WavAudioFormat::Writer>(
            *file_output_stream_, rav::WavAudioFormat::FormatCode::pcm, audio_format_.sample_rate,
            audio_format_.num_channels, audio_format_.bytes_per_sample() * 8
        );
        audio_data_.resize(parameters.streams.front().packet_time_frames * audio_format_.bytes_per_frame());
    }

    void on_data_ready(const rav::WrappingUint32 timestamp) override {
        TRACY_ZONE_SCOPED;

        if (!receiver_->read_data_realtime(audio_data_.data(), audio_data_.size(), timestamp.value())) {
            RAV_ERROR("Failed to read audio data");
            return;
        }
        if (audio_format_.byte_order == rav::AudioFormat::ByteOrder::be) {
            rav::swap_bytes(audio_data_.data(), audio_data_.size(), audio_format_.bytes_per_sample());
        }
        if (!wav_writer_->write_audio_data(audio_data_.data(), audio_data_.size())) {
            RAV_ERROR("Failed to write audio data");
        }
    }

  private:
    std::unique_ptr<rav::RavennaReceiver> receiver_;
    std::unique_ptr<rav::FileOutputStream> file_output_stream_;
    std::unique_ptr<rav::WavAudioFormat::Writer> wav_writer_;
    std::vector<uint8_t> audio_data_;
    rav::AudioFormat audio_format_;
    std::optional<rav::WrappingUint32> stream_ts_;
};

class RavennaRecorder {
  public:
    explicit RavennaRecorder() {
        rtsp_client_ = std::make_unique<rav::RavennaRtspClient>(io_context_, browser_);
        rtp_receiver_ = std::make_unique<rav::rtp::Receiver3>(io_context_);
    }

    ~RavennaRecorder() = default;

    void add_stream(const std::string& stream_name, const rav::NetworkInterfaceConfig& network_interface_config) {
        rav::RavennaReceiver::Configuration config;
        config.delay_frames = 480;  // 10ms at 48KHz
        config.enabled = true;
        config.session_name = stream_name;

        auto receiver = std::make_unique<rav::RavennaReceiver>(
            io_context_, *rtsp_client_, *rtp_receiver_, rav::Id::get_next_process_wide_unique_id()
        );
        receiver->set_network_interface_config(network_interface_config);
        auto result = receiver->set_configuration(config);
        if (!result) {
            RAV_ERROR("Failed to update configuration: {}", result.error());
            return;
        }
        recorders_.emplace_back(std::make_unique<StreamRecorder>(std::move(receiver)));
    }

    void run() {
        while (!io_context_.stopped()) {
            io_context_.poll();
        }
    }

    void stop() {
        io_context_.stop();
    }

  private:
    boost::asio::io_context io_context_;
    rav::RavennaBrowser browser_ {io_context_};
    std::unique_ptr<rav::RavennaRtspClient> rtsp_client_;
    std::unique_ptr<rav::rtp::Receiver3> rtp_receiver_;
    std::vector<std::unique_ptr<StreamRecorder>> recorders_;
};

}  // namespace examples

/**
 * This examples demonstrates how to receive audio streams from a RAVENNA device and write the audio data to wav files.
 * It sets up a RAVENNA sink that listens for announcements from a RAVENNA device and starts receiving audio data.
 * Separate files for each stream are created and existing files will be overwritten.
 * Note: this examples shows custom implementation of sending streams, the easier, higher level and recommended approach
 * is to use the RavennaNode class (see ravenna_node_example).
 */
int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Recorder example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> stream_names;
    app.add_option("stream_names", stream_names, "The names of the streams to receive")->required();

    std::string interface;
    app.add_option(
           "--interface", interface,
           "The interface address to use. The value can be the identifier, display name, description, MAC or an ip address."
    )
        ->required();

    CLI11_PARSE(app, argc, argv);

    auto* iface = rav::NetworkInterfaceList::get_system_interfaces().find_by_string(interface);
    if (iface == nullptr) {
        RAV_ERROR("No network interface found with search string: {}", interface);
        return -1;
    }

    rav::NetworkInterfaceConfig interface_config;
    interface_config.set_interface(rav::Rank::primary(), iface->get_identifier());

    examples::RavennaRecorder recorder_example;

    for (auto& stream_name : stream_names) {
        recorder_example.add_stream(stream_name, interface_config);
    }

    std::thread cin_thread([&recorder_example] {
        fmt::println("Press return key to stop...");
        std::cin.get();
        recorder_example.stop();
    });

    recorder_example.run();
    cin_thread.join();

    return 0;
}
