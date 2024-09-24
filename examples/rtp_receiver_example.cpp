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

#include <CLI/CLI.hpp>
#include <uvw.hpp>

#include "portaudio.h"
#include "ravennakit/util/tracy.hpp"
#include "ravennakit/audio/circular_audio_buffer.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/rtp/rtp_receiver.hpp"

constexpr short k_port = 5004;
constexpr int k_block_size = 256;
constexpr int k_num_channels = 2;
constexpr double k_sample_rate = 48000.0;

using audio_buffer_t = rav::circular_audio_buffer<float, rav::fifo::spsc>;

static int audio_callback(
    [[maybe_unused]] const void* input, void* output, const unsigned long num_frames,
    [[maybe_unused]] const PaStreamCallbackTimeInfo* time_info, [[maybe_unused]] PaStreamCallbackFlags status,
    void* user_data
) {
    ZoneScoped;

    if (num_frames != k_block_size) {
        RAV_ERROR("Unexpected number of frames: {}", num_frames);
    }
    auto* buffer = static_cast<audio_buffer_t*>(user_data);
    const auto result =
        buffer->read_to_data<float, rav::audio_data::byte_order::ne, rav::audio_data::interleaving::interleaved>(
            static_cast<float*>(output), num_frames
        );
    if (!result) {
        RAV_ERROR("Failed to read from buffer!");
        std::memset(output, 0, num_frames * k_num_channels * sizeof(float));
    }

    return 0;
}

int main(int const argc, char* argv[]) {
#ifdef RAV_ENABLE_SPDLOG
    spdlog::set_level(spdlog::level::trace);
#endif

    CLI::App app {"App description"};
    argv = app.ensure_utf8(argv);

    std::string listen_addr = "0.0.0.0";
    app.add_option("listen-addr", listen_addr, "The listen address");

    std::optional<std::string> multicast_addr;
    app.add_option("multicast-addr", multicast_addr, "The multicast address to receive from");

    std::optional<std::string> multicast_interface;
    app.add_option("multicast-interface", multicast_interface, "The multicast interface to receive from");

    std::optional<std::string> audio_output_device_name = "default";
    app.add_option(
        "--audio-device", audio_output_device_name,
        "The name of the audio output device. Uses the default device if not specified"
    );

    CLI11_PARSE(app, argc, argv);

    if (auto error = Pa_Initialize(); error != paNoError) {
        RAV_ERROR("PortAudio failed to initialize! Error: {}", Pa_GetErrorText(error));
        exit(0);
    }

    const auto loop = uvw::loop::create();
    if (loop == nullptr) {
        return 1;
    }

    audio_buffer_t audio_buffer(k_num_channels, k_block_size * 20);

    rav::audio_buffer<int16_t> converted_int16(k_num_channels, k_block_size);
    rav::audio_buffer<float> converted_float(k_num_channels, k_block_size);
    std::vector<int16_t> audio_data_buffer(k_num_channels * k_block_size);

    rav::rtp_receiver receiver(loop);
    receiver.on<rav::rtp_packet_event>([&](const rav::rtp_packet_event& event,
                                           [[maybe_unused]] rav::rtp_receiver& recv) {
        RAV_INFO("{}", event.packet.to_string());

        const auto payload = event.packet.payload_data().reinterpret<const int16_t>();
        const auto num_frames = payload.size() / k_num_channels;

        const auto result =
            audio_buffer
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
            receiver.set_multicast_membership(
                *multicast_addr, *multicast_interface, uvw::udp_handle::membership::JOIN_GROUP
            );
        } else {
            receiver.set_multicast_membership(*multicast_addr, {}, uvw::udp_handle::membership::JOIN_GROUP);
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
            // break;
        }
    }

    if (selected_device < 0) {
        RAV_ERROR("Failed to find audio device with name: {}", *audio_output_device_name);
        exit(1);
    }

    PaStreamParameters output_params;
    output_params.device = selected_device;
    output_params.channelCount = k_num_channels;
    output_params.sampleFormat = paFloat32;  // | paNonInterleaved;
    output_params.suggestedLatency = Pa_GetDeviceInfo(selected_device)->defaultHighOutputLatency;
    output_params.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream;
    if (auto error = Pa_OpenStream(
            &stream, nullptr, &output_params, k_sample_rate, k_block_size, paNoFlag, audio_callback, &audio_buffer
        );
        error != paNoError) {
        RAV_ERROR("PortAudio failed to open stream! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    if (auto error = Pa_StartStream(stream); error != paNoError) {
        RAV_ERROR("PortAudio failed to start stream! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    const auto signal = loop->resource<uvw::signal_handle>();
    signal->on<uvw::signal_event>([&receiver, &signal](const uvw::signal_event&, uvw::signal_handle&) {
        receiver.close();
        signal->close();  // Need to close ourselves, otherwise the loop will not stop.
    });
    signal->start(SIGTERM);

    auto result = loop->run();

    if (auto error = Pa_StopStream(stream); error != paNoError) {
        RAV_ERROR("PortAudio failed to stop stream! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    if (auto error = Pa_Terminate(); error != paNoError) {
        RAV_ERROR("PortAudio failed to terminate! Error: {}", Pa_GetErrorText(error));
        exit(1);
    }

    return result;
}
