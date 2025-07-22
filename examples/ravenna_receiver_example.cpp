/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/core/util/tracy.hpp"
#include "ravennakit/dnssd/bonjour/bonjour_browser.hpp"
#include "ravennakit/ravenna/ravenna_node.hpp"
#include "ravennakit/ravenna/ravenna_rtsp_client.hpp"
#include "ravennakit/ravenna/ravenna_receiver.hpp"

#include <portaudio.h>
#include <CLI/App.hpp>
#include <boost/asio/io_context.hpp>
#include <utility>

namespace {
constexpr int k_block_size = 32;   // Frames
constexpr uint32_t k_delay = 240;  // Frames
static_assert(k_delay > k_block_size * 2, "Delay should at least be 2 block sizes");

void portaudio_iterate_devices(const std::function<bool(PaDeviceIndex index, const PaDeviceInfo&)>& callback) {
    const auto num_devices = Pa_GetDeviceCount();
    if (num_devices < 0) {
        RAV_THROW_EXCEPTION("PortAudio failed to get device count! Error: {}", Pa_GetErrorText(num_devices));
    }
    for (int i = 0; i < num_devices; ++i) {
        const auto info = Pa_GetDeviceInfo(i);
        if (info == nullptr) {
            RAV_THROW_EXCEPTION("PortAudio failed to get device info for device index: {}", i);
        }
        if (callback(i, *info) == false) {
            return;
        }
    }
}

std::optional<PaDeviceIndex> portaudio_find_device_index_for_name(const std::string& device_name) {
    PaDeviceIndex device_index = -1;

    portaudio_iterate_devices([&device_name, &device_index](const PaDeviceIndex index, const PaDeviceInfo& device) {
        if (device_name == device.name) {
            device_index = index;
            return false;
        }
        return true;
    });

    if (device_index < 0) {
        return std::nullopt;
    }

    return device_index;
}

void portaudio_print_devices() {
    portaudio_iterate_devices([](PaDeviceIndex index, const PaDeviceInfo& info) {
        RAV_INFO("[{}]: {}", index, info.name);
        return true;
    });
}

std::optional<uint32_t> portaudio_get_sample_format_for_audio_format(const rav::AudioFormat& audio_format) {
    std::array<std::pair<rav::AudioEncoding, uint32_t>, 5> pairs {
        {{rav::AudioEncoding::pcm_u8, paUInt8},
         {rav::AudioEncoding::pcm_s8, paInt8},
         {rav::AudioEncoding::pcm_s16, paInt16},
         {rav::AudioEncoding::pcm_s24, paInt24},
         {rav::AudioEncoding::pcm_s32, paInt32}},
    };
    for (auto& pair : pairs) {
        if (pair.first == audio_format.encoding) {
            return pair.second;
        }
    }
    return std::nullopt;
}

class Portaudio {
  public:
    static void init() {
        static Portaudio instance;
        std::ignore = instance;
    }

  private:
    Portaudio() {
        if (const auto error = Pa_Initialize(); error != paNoError) {
            RAV_THROW_EXCEPTION("PortAudio failed to initialize! Error: {}", Pa_GetErrorText(error));
        }
    }

    ~Portaudio() {
        if (const auto error = Pa_Terminate(); error != paNoError) {
            RAV_ERROR("PortAudio failed to terminate! Error: {}", Pa_GetErrorText(error));
        }
    }
};

class PortaudioStream {
  public:
    PortaudioStream() {
        Portaudio::init();
    }

    ~PortaudioStream() {
        close();
    }

    void open_output_stream(
        const std::string& audio_device_name, const double sample_rate, const int channel_count,
        const uint32_t sample_format, PaStreamCallback* callback, void* user_data
    ) {
        close();

        const auto device_index = portaudio_find_device_index_for_name(audio_device_name);
        if (device_index == std::nullopt) {
            RAV_THROW_EXCEPTION("Audio device not found: {}", audio_device_name);
        }

        PaStreamParameters output_params;
        output_params.device = *device_index;
        output_params.channelCount = channel_count;
        output_params.sampleFormat = sample_format;
        output_params.suggestedLatency = Pa_GetDeviceInfo(*device_index)->defaultLowOutputLatency;
        output_params.hostApiSpecificStreamInfo = nullptr;

        const auto error =
            Pa_OpenStream(&stream_, nullptr, &output_params, sample_rate, k_block_size, paNoFlag, callback, user_data);

        if (error != paNoError) {
            RAV_THROW_EXCEPTION("PortAudio failed to open stream! Error: {}", Pa_GetErrorText(error));
        }

        start();

        RAV_TRACE("Opened PortAudio stream for device: {}", audio_device_name);
    }

    void start() const {
        const auto error = Pa_StartStream(stream_);
        if (error != paNoError) {
            RAV_THROW_EXCEPTION("PortAudio failed to start stream! Error: {}", Pa_GetErrorText(error));
        }
    }

    void stop() const {
        if (stream_ == nullptr) {
            return;
        }
        if (const auto error = Pa_StopStream(stream_); error != paNoError) {
            RAV_ERROR("PortAudio failed to stop stream! Error: {}", Pa_GetErrorText(error));
        }
    }

    void close() {
        if (stream_ == nullptr) {
            return;
        }
        if (const auto error = Pa_CloseStream(stream_); error != paNoError) {
            RAV_ERROR("PortAudio failed to close stream! Error: {}", Pa_GetErrorText(error));
        }
        stream_ = nullptr;
    }

  private:
    PaStream* stream_ = nullptr;
};

class RavennaReceiverExample: public rav::RavennaReceiver::Subscriber, public rav::ptp::Instance::Subscriber {
  public:
    explicit RavennaReceiverExample(
        rav::RavennaNode& ravenna_node, const std::string& stream_name, std::string audio_device_name
    ) :
        ravenna_node_(ravenna_node), audio_device_name_(std::move(audio_device_name)) {
        rav::RavennaReceiver::Configuration config;
        config.enabled = true;
        config.session_name = stream_name;
        // No need to set the delay, because RAVENNAKIT doesn't use this value

        auto id = ravenna_node_.create_receiver(config).get();
        if (!id) {
            RAV_THROW_EXCEPTION("Failed to create receiver: {}", id.error());
        }
        receiver_id_ = *id;

        ravenna_node_.subscribe_to_receiver(receiver_id_, this).wait();
        ravenna_node_.subscribe_to_ptp_instance(this).wait();
    }

    ~RavennaReceiverExample() override {
        ravenna_node_.unsubscribe_from_ptp_instance(this).wait();
        if (receiver_id_.is_valid()) {
            ravenna_node_.unsubscribe_from_receiver(receiver_id_, this).wait();
            ravenna_node_.remove_receiver(receiver_id_).wait();
        }
    }

    void ravenna_receiver_parameters_updated(const rav::rtp::AudioReceiver::ReaderParameters& parameters) override {
        if (parameters.streams.empty()) {
            RAV_WARNING("No streams available");
            return;
        }

        if (!parameters.audio_format.is_valid() || audio_format_ == parameters.audio_format) {
            return;
        }

        audio_format_ = parameters.audio_format;
        const auto sample_format = portaudio_get_sample_format_for_audio_format(audio_format_);
        if (!sample_format.has_value()) {
            RAV_TRACE("Skipping stream update because audio format is invalid: {}", audio_format_.to_string());
            return;
        }
        portaudio_stream_.open_output_stream(
            audio_device_name_, audio_format_.sample_rate, static_cast<int>(audio_format_.num_channels), *sample_format,
            &RavennaReceiverExample::stream_callback, this
        );
    }

  private:
    rav::RavennaNode& ravenna_node_;
    std::string audio_device_name_;
    PortaudioStream portaudio_stream_;
    rav::AudioFormat audio_format_;
    rav::Id receiver_id_;

    int stream_callback(
        const void* input, void* output, const unsigned long frame_count, const PaStreamCallbackTimeInfo* time_info,
        const PaStreamCallbackFlags status_flags
    ) {
        TRACY_ZONE_SCOPED;

        std::ignore = input;
        std::ignore = time_info;
        std::ignore = status_flags;

        const auto buffer_size = frame_count * audio_format_.bytes_per_frame();

        auto& local_clock = get_local_clock();
        if (!local_clock.is_calibrated()) {
            // As long as the PTP clock is not stable, we will output silence
            std::memset(output, audio_format_.ground_value(), buffer_size);
            return paContinue;
        }

        const auto ptp_ts =
            static_cast<uint32_t>(get_local_clock().now().to_samples(audio_format_.sample_rate)) - k_delay;

        // First we try to read data
        auto rtp_ts =
            ravenna_node_.read_data_realtime(receiver_id_, static_cast<uint8_t*>(output), buffer_size, {}, {});

        // If reading data fails (which is expected when no audio callbacks have been made for a while) we output
        // silence.
        if (!rtp_ts) {
            std::memset(output, audio_format_.ground_value(), buffer_size);
            return paContinue;
        }

        auto drift = rav::WrappingUint32(ptp_ts).diff(rav::WrappingUint32(*rtp_ts));

        // If the drift becomes too big, we reset the timestamp to the current time to realign incoming data with the
        // audio callbacks
        if (static_cast<uint32_t>(std::abs(drift)) > frame_count * 2) {
            rtp_ts =
                ravenna_node_.read_data_realtime(receiver_id_, static_cast<uint8_t*>(output), buffer_size, ptp_ts, {});
            RAV_WARNING("Re-aligned stream by {} samples", -drift);
            drift = rav::WrappingUint32(ptp_ts).diff(rav::WrappingUint32(*rtp_ts));
        }

        TRACY_PLOT("drift", static_cast<double>(drift));

        if (audio_format_.byte_order == rav::AudioFormat::ByteOrder::be) {
            rav::swap_bytes(static_cast<uint8_t*>(output), buffer_size, audio_format_.bytes_per_sample());
        }

        return paContinue;
    }

    static int stream_callback(
        const void* input, void* output, const unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo,
        const PaStreamCallbackFlags statusFlags, void* userData
    ) {
        RAV_ASSERT(userData, "userData is null");
        return static_cast<RavennaReceiverExample*>(userData)->stream_callback(
            input, output, frameCount, timeInfo, statusFlags
        );
    }
};

}  // namespace

/**
 * This examples demonstrates how to receive audio streams from a RAVENNA device. It sets up a RAVENNA sink that listens
 * for announcements from a RAVENNA device and starts receiving audio data. It will play the audio to the selected audio
 * device using portaudio.
 * Warning! No drift correction is done between the sender and receiver. At some point buffers will overflow or
 * underflow.
 * Note: this examples shows custom implementation of sending streams, the easier, higher level and recommended approach
 * is to use the RavennaNode class (see ravenna_node_example).
 */
int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    Portaudio::init();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::string stream_name;
    app.add_option("stream_name", stream_name, "The name of the stream to receive")->required();

    std::string audio_output_device;
    app.add_option("audio_output_device", audio_output_device, "The name of the audio output device")->required();

    std::string interface;
    app.add_option(
        "--interface", interface,
        "The interface address to use. The value can be the identifier, display name, description, MAC or an ip address."
    );

    CLI11_PARSE(app, argc, argv);

    auto* iface = rav::NetworkInterfaceList::get_system_interfaces().find_by_string(interface);
    if (iface == nullptr) {
        RAV_ERROR("No network interface found with search string: {}", interface);
        return -1;
    }

    portaudio_print_devices();

    rav::NetworkInterfaceConfig network_interface_config;
    network_interface_config.set_interface(rav::Rank::primary(), iface->get_identifier());

    rav::RavennaNode node;
    node.set_network_interface_config(network_interface_config).wait();

    RavennaReceiverExample example(node, stream_name, audio_output_device);

    fmt::println("Press return key to stop...");
    std::string line;
    std::getline(std::cin, line);

    return 0;
}
