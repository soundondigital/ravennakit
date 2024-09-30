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
#include "ravennakit/audio/circular_audio_buffer.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/rtp/rtp_receiver.hpp"
#include "ravennakit/util/tracy.hpp"

constexpr short k_port = 5004;
constexpr int k_block_size = 256;

struct audio_context {
    rav::circular_audio_buffer<float, rav::fifo::spsc> buffer;
    size_t num_channels;
};

static int audio_callback(
    [[maybe_unused]] const void* input, void* output, const unsigned long num_frames,
    [[maybe_unused]] const PaStreamCallbackTimeInfo* time_info, [[maybe_unused]] PaStreamCallbackFlags status,
    void* user_data
) {
    ZoneScoped;

    if (num_frames != k_block_size) {
        RAV_ERROR("Unexpected number of frames: {}", num_frames);
    }
    auto* context = static_cast<audio_context*>(user_data);
    const auto result =
        context->buffer
            .read_to_data<float, rav::audio_data::byte_order::ne, rav::audio_data::interleaving::interleaved>(
                static_cast<float*>(output), num_frames
            );
    if (!result) {
        RAV_ERROR("Failed to read from buffer!");
        std::memset(output, 0, num_frames * context->num_channels * sizeof(float));
    }

    return 0;
}

int main(int const argc, char* argv[]) {
#ifdef RAV_ENABLE_SPDLOG
    spdlog::set_level(spdlog::level::trace);
#endif

    rav::system::do_system_checks();

    CLI::App app {"App description"};
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

    audio_context audio_context {
        rav::circular_audio_buffer<float, rav::fifo::spsc>(num_channels, k_block_size * 20), num_channels
    };

    if (auto error = Pa_Initialize(); error != paNoError) {
        RAV_ERROR("PortAudio failed to initialize! Error: {}", Pa_GetErrorText(error));
        exit(0);
    }

    io_context_runner io_context_runner;

    rav::rtp_receiver receiver(io_context_runner.io_context());

    asio::signal_set signals(io_context_runner.io_context(), SIGINT, SIGTERM);

    receiver.on<rav::rtp_packet_event>([&audio_context](
                                           const rav::rtp_packet_event& event, [[maybe_unused]] rav::rtp_receiver& recv
                                       ) {
        RAV_INFO("{}", event.packet.to_string());

        const auto payload = event.packet.payload_data().reinterpret<const int16_t>();
        const auto num_frames = payload.size() / audio_context.num_channels;

        const auto result =
            audio_context.buffer
                .write_from_data<int16_t, rav::audio_data::byte_order::be, rav::audio_data::interleaving::interleaved>(
                    payload.data(), num_frames
                );
        if (!result) {
            RAV_ERROR("Failed to write {} frames to buffer!", num_frames);
        }
    });
    receiver.on<rav::rtcp_packet_event>([](const rav::rtcp_packet_event& event,
                                           [[maybe_unused]] rav::rtp_receiver& recv) {
        fmt::println("{}", event.packet.to_string());
    });

    receiver.bind(listen_addr, k_port);

    if (multicast_addr.has_value()) {
        if (multicast_interface.has_value()) {
            receiver.join_multicast_group(*multicast_addr, *multicast_interface);
        } else {
            receiver.join_multicast_group(*multicast_addr, {});
        }
    }

    receiver.start();

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
    output_params.channelCount = static_cast<int>(audio_context.num_channels);
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = Pa_GetDeviceInfo(selected_device)->defaultHighOutputLatency;
    output_params.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream;
    if (auto error = Pa_OpenStream(
            &stream, nullptr, &output_params, sample_rate, k_block_size, paNoFlag, audio_callback, &audio_context
        );
        error != paNoError) {
        RAV_ERROR("PortAudio failed to open stream! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    if (auto error = Pa_StartStream(stream); error != paNoError) {
        RAV_ERROR("PortAudio failed to start stream! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    signals.async_wait([&receiver](const std::error_code&, int) {
        receiver.stop();
    });

    io_context_runner.run_to_completion();

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
