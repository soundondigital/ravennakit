/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <fmt/core.h>
#include <portaudio.h>

#include <CLI/CLI.hpp>
#include <optional>

#include "ravennakit/asio/io_context_runner.hpp"
#include "ravennakit/core/audio/circular_audio_buffer.hpp"
#include "ravennakit/core/audio/formats/wav_audio_format.hpp"
#include "ravennakit/core/streams/file_output_stream.hpp"
#include "ravennakit/core/file.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/rtp/rtp_receiver.hpp"
#include "ravennakit/core/tracy.hpp"

constexpr short k_port = 5004;
constexpr int k_block_size = 256;

class example_receiver final: public rav::rtp_receiver::subscriber {
  public:
    explicit example_receiver(
        rav::rtp_receiver& rtp_receiver, const size_t num_channels, const rav::file& audio_file,
        const double sample_rate
    ) :
        buffer_(num_channels, k_block_size * 20),
        num_channels_(num_channels),
        audio_file_stream(audio_file),
        audio_writer_(audio_file_stream, rav::wav_audio_format::format_code::pcm, sample_rate, num_channels, 16) {
        subscribe(rtp_receiver);
    }

    void on(const rav::rtp_receiver::rtp_packet_event& event) override {
        RAV_INFO("{}", event.packet.to_string());

        auto payload = event.packet.payload_data();
        audio_data_buffer_.resize(payload.size());
        std::memcpy(audio_data_buffer_.data(), payload.data(), payload.size());
        rav::byte_order::swap_bytes(audio_data_buffer_.data(), audio_data_buffer_.size(), sizeof(int16_t));
        audio_writer_.write_audio_data(audio_data_buffer_.data(), audio_data_buffer_.size());

        const auto audio_payload = payload.reinterpret<const int16_t>();
        const auto num_frames = audio_payload.size() / num_channels_;

        const auto result =
            buffer_
                .write_from_data<int16_t, rav::audio_data::byte_order::be, rav::audio_data::interleaving::interleaved>(
                    audio_payload.data(), num_frames
                );

        if (!result) {
            RAV_ERROR("Failed to write {} frames to buffer!", num_frames);
        }
    }

    void on(const rav::rtp_receiver::rtcp_packet_event& event) override {
        fmt::println("{}", event.packet.to_string());
    }

    static int audio_callback(
        [[maybe_unused]] const void* input, void* output, const unsigned long num_frames,
        [[maybe_unused]] const PaStreamCallbackTimeInfo* time_info, [[maybe_unused]] PaStreamCallbackFlags status,
        void* user_data
    ) {
        TRACY_ZONE_SCOPED;

        if (num_frames != k_block_size) {
            RAV_ERROR("Unexpected number of frames: {}", num_frames);
        }
        auto* context = static_cast<example_receiver*>(user_data);
        const auto result =
            context->buffer_
                .read_to_data<float, rav::audio_data::byte_order::ne, rav::audio_data::interleaving::interleaved>(
                    static_cast<float*>(output), num_frames
                );
        if (!result) {
            RAV_ERROR("Failed to read from buffer!");
            std::memset(output, 0, num_frames * context->num_channels_ * sizeof(float));
        }

        return 0;
    }

    [[nodiscard]] size_t num_channels() const {
        return num_channels_;
    }

  private:
    std::vector<uint8_t> audio_data_buffer_;
    rav::circular_audio_buffer<float, rav::fifo::spsc> buffer_;
    size_t num_channels_ {};
    rav::file_output_stream audio_file_stream;
    rav::wav_audio_format::writer audio_writer_;
};

int main(int const argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    CLI::App app {"RTP Receiver example"};
    argv = app.ensure_utf8(argv);

    std::string listen_addr = "0.0.0.0";
    app.add_option("listen-addr", listen_addr, "The listen address");

    std::optional<std::string> multicast_addr;
    app.add_option("multicast-addr", multicast_addr, "The multicast address to receive from (optional)");

    std::optional<std::string> multicast_interface;
    app.add_option("multicast-interface", multicast_interface, "The multicast interface to receive from (optional)");

    size_t num_channels = 0;
    app.add_option("-c,--num-channels", num_channels, "The number of channels in the RTP stream")->required();

    double sample_rate = 0.0;
    app.add_option("-r,--sample-rate", sample_rate, "The sample rate of the RTP stream")->required();

    std::optional<std::string> audio_output_device_name = "default";
    app.add_option(
        "-o,--out-device", audio_output_device_name,
        "The name of the audio output device. Uses the default device if not specified"
    );

    CLI11_PARSE(app, argc, argv);

    if (num_channels == 0) {
        RAV_ERROR("Number of channels must be greater than 0!");
        exit(1);
    }

    if (auto error = Pa_Initialize(); error != paNoError) {
        RAV_ERROR("PortAudio failed to initialize! Error: {}", Pa_GetErrorText(error));
        exit(0);
    }

    auto example_dir = rav::file(argv[0]).parent();
    auto audio_file = example_dir / "audio.wav";
    fmt::println("Audio file: {}", audio_file.path().string());

    asio::io_context io_context;

    rav::rtp_receiver rtp_receiver(io_context);
    rtp_receiver.bind(listen_addr, k_port);

    if (multicast_addr.has_value()) {
        if (multicast_interface.has_value()) {
            rtp_receiver.join_multicast_group(*multicast_addr, *multicast_interface);
        } else {
            rtp_receiver.join_multicast_group(*multicast_addr, {});
        }
    }

    example_receiver example_receiver(rtp_receiver, num_channels, audio_file, sample_rate);

    auto num_devices = Pa_GetDeviceCount();
    if (num_devices < 0) {
        RAV_ERROR("PortAudio failed to get device count! Error: {}", Pa_GetErrorText(num_devices));
        exit(1);
    }

    int selected_device = -1;
    for (int i = 0; i < num_devices; ++i) {
        auto info = Pa_GetDeviceInfo(i);
        RAV_INFO("Device: {}", info->name);
        if (info->name != nullptr && audio_output_device_name == info->name) {
            selected_device = i;
        }
    }

    if (selected_device < 0) {
        RAV_ERROR("Failed to find audio device with name: {}", *audio_output_device_name);
        exit(1);
    }

    PaStreamParameters output_params;
    output_params.device = selected_device;
    output_params.channelCount = static_cast<int>(example_receiver.num_channels());
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = Pa_GetDeviceInfo(selected_device)->defaultHighOutputLatency;
    output_params.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream;
    if (auto error = Pa_OpenStream(
            &stream, nullptr, &output_params, sample_rate, k_block_size, paNoFlag, example_receiver::audio_callback,
            &example_receiver
        );
        error != paNoError) {
        RAV_ERROR("PortAudio failed to open stream! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    if (auto error = Pa_StartStream(stream); error != paNoError) {
        RAV_ERROR("PortAudio failed to start stream! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    rtp_receiver.start();

    asio::signal_set signals(io_context, SIGINT, SIGTERM);

    signals.async_wait([&rtp_receiver](const std::error_code&, int) {
        rtp_receiver.stop();
    });

    std::thread io_thread([&io_context] {
        io_context.run();
    });

    fmt::println("Press return key to stop...");
    std::cin.get();

    rtp_receiver.stop();
    signals.cancel();
    io_thread.join();

    if (auto error = Pa_StopStream(stream); error != paNoError) {
        RAV_ERROR("PortAudio failed to stop stream! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    if (auto error = Pa_Terminate(); error != paNoError) {
        RAV_ERROR("PortAudio failed to terminate! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    return 0;
}
