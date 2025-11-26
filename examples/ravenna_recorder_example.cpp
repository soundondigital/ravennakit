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
#include "ravennakit/ravenna/ravenna_node.hpp"
#include "ravennakit/ravenna/ravenna_rtsp_client.hpp"
#include "ravennakit/ravenna/ravenna_receiver.hpp"

#include <CLI/App.hpp>
#include <boost/asio/io_context.hpp>
#include <utility>

namespace {

/**
 * A class that is a subscriber to a RavennaReceiver and writes the audio data to a wav file.
 */
class StreamRecorder: public rav::RavennaReceiver::Subscriber {
  public:
    static constexpr auto k_delay_ms = 10;
    static constexpr auto k_block_size = 512;  // Num frames per read/write

    explicit StreamRecorder(rav::RavennaNode& ravenna_node, const rav::Id receiver_id) :
        ravenna_node_(ravenna_node), receiver_id_(receiver_id) {
        RAV_ASSERT(receiver_id.is_valid(), "Invalid id");
        ravenna_node.subscribe_to_receiver(receiver_id_, this).wait();
    }

    ~StreamRecorder() override {
        ravenna_node_.unsubscribe_from_receiver(receiver_id_, this).wait();
        close();
    }

    void close() {
        const bool is_recording = wav_writer_.get() != nullptr;
        if (wav_writer_) {
            if (!wav_writer_->finalize()) {
                RAV_LOG_ERROR("Failed to finalize wav file");
            }
            wav_writer_.reset();
        }
        if (file_output_stream_) {
            file_output_stream_.reset();
        }
        if (is_recording) {
            RAV_LOG_INFO("Closed audio recording");
        }
    }

    void ravenna_receiver_parameters_updated(const rav::rtp::AudioReceiver::ReaderParameters& parameters) override {
        RAV_ASSERT_NODE_MAINTENANCE_THREAD(ravenna_node_);

        std::lock_guard guard(mutex_);

        close();

        if (!parameters.is_valid()) {
            return;
        }

        if (parameters.streams.empty()) {
            RAV_LOG_WARNING("No streams available");
            return;
        }

        if (!parameters.audio_format.is_valid()) {
            RAV_LOG_WARNING("Invalid audio format");
            return;
        }

        audio_format_ = parameters.audio_format;

        if (!session_name_.empty()) {
            start_recording();
        }
    }

    void ravenna_receiver_configuration_updated(
        const rav::RavennaReceiver& receiver, const rav::RavennaReceiver::Configuration& configuration
    ) override {
        std::ignore = receiver;

        RAV_ASSERT_NODE_MAINTENANCE_THREAD(ravenna_node_);

        std::lock_guard guard(mutex_);

        if (configuration.session_name == session_name_) {
            return;
        }

        session_name_ = configuration.session_name;

        if (session_name_.empty()) {
            close();
            return;
        }

        if (audio_format_.is_valid()) {
            start_recording();
        }
    }

    void process_audio() {
        std::lock_guard guard(mutex_);

        if (audio_data_.empty()) {
            return;
        }

        for (int i = 0; i < 10; ++i) {
            const auto ts = ravenna_node_.read_data_realtime(receiver_id_, audio_data_.data(), audio_data_.size(), std::nullopt, delay_);
            if (!ts) {
                return;
            }

            if (audio_format_.byte_order == rav::AudioFormat::ByteOrder::be) {
                rav::swap_bytes(audio_data_.data(), audio_data_.size(), audio_format_.bytes_per_sample());
            }
            if (!wav_writer_->write_audio_data(audio_data_.data(), audio_data_.size())) {
                RAV_LOG_ERROR("Failed to write audio data");
            }
        }
    }

  private:
    rav::RavennaNode& ravenna_node_;
    const rav::Id receiver_id_;

    std::mutex mutex_;
    std::string session_name_;
    std::unique_ptr<rav::FileOutputStream> file_output_stream_;
    std::unique_ptr<rav::WavAudioFormat::Writer> wav_writer_;
    std::vector<uint8_t> audio_data_;
    rav::AudioFormat audio_format_;
    uint32_t delay_ = 0;

    void start_recording() {
        if (!audio_format_.is_valid()) {
            RAV_LOG_ERROR("Invalid audio format");
            return;
        }

        if (session_name_.empty()) {
            RAV_LOG_ERROR("No session name");
            return;
        }

        auto file_path = std::filesystem::absolute(session_name_ + ".wav");

        RAV_LOG_INFO("Start recording stream to: \"{}\" to file: {}", session_name_, file_path.string());

        file_output_stream_ = std::make_unique<rav::FileOutputStream>(file_path);
        wav_writer_ = std::make_unique<rav::WavAudioFormat::Writer>(
            *file_output_stream_, rav::WavAudioFormat::FormatCode::pcm, audio_format_.sample_rate, audio_format_.num_channels,
            audio_format_.bytes_per_sample() * 8
        );
        audio_data_.resize(k_block_size * audio_format_.bytes_per_frame());
        delay_ = audio_format_.sample_rate * k_delay_ms / 1000;
    }
};

}  // namespace

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
        RAV_LOG_ERROR("No network interface found with search string: {}", interface);
        return -1;
    }

    rav::NetworkInterfaceConfig interface_config;
    interface_config.set_interface(rav::rank::primary, iface->get_identifier());

    rav::RavennaNode node;
    node.set_network_interface_config(std::move(interface_config)).get();
    std::vector<std::unique_ptr<StreamRecorder>> recorders;

    for (const auto& stream_name : stream_names) {
        rav::RavennaReceiver::Configuration config;
        config.delay_frames = 480;  // 10ms at 48KHz
        config.enabled = true;
        config.session_name = stream_name;
        auto id = node.create_receiver(config).get();
        if (!id) {
            RAV_LOG_ERROR("Failed to create receiver: {}", id.error());
            return -1;
        }
        recorders.emplace_back(std::make_unique<StreamRecorder>(node, *id));
    }

    std::atomic keep_going {true};

    std::thread recorder_thread([&] {
        while (keep_going.load(std::memory_order_relaxed)) {
            for (const auto& recorder : recorders) {
                recorder->process_audio();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));  // Keep this small enough
        }
    });

    fmt::println("Press return key to stop...");

    std::string line;
    std::getline(std::cin, line);

    keep_going.store(false, std::memory_order_relaxed);

    recorder_thread.join();

    return 0;
}
