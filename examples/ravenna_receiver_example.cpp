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
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/dnssd/bonjour/bonjour_browser.hpp"
#include "ravennakit/ravenna/ravenna_rtsp_client.hpp"
#include "ravennakit/ravenna/ravenna_receiver.hpp"

#include <portaudio.h>
#include <CLI/App.hpp>
#include <asio/io_context.hpp>
#include <utility>

namespace examples {
constexpr int k_block_size = 256;

class portaudio {
  public:
    static void init() {
        static portaudio instance;
        std::ignore = instance;
    }

  private:
    portaudio() {
        if (const auto error = Pa_Initialize(); error != paNoError) {
            RAV_THROW_EXCEPTION("PortAudio failed to initialize! Error: {}", Pa_GetErrorText(error));
        }
    }

    ~portaudio() {
        if (const auto error = Pa_Terminate(); error != paNoError) {
            RAV_ERROR("PortAudio failed to terminate! Error: {}", Pa_GetErrorText(error));
        }
    }
};

class portaudio_stream {
  public:
    portaudio_stream() {
        portaudio::init();
    }

    ~portaudio_stream() {
        close();
    }

    void open_output_stream(
        const std::string& audio_device_name, const double sample_rate, const int channel_count,
        const uint32_t sample_format, PaStreamCallback* callback, void* user_data
    ) {
        close();

        const auto device_index = find_device_index_for_name(audio_device_name);
        if (device_index == std::nullopt) {
            RAV_THROW_EXCEPTION("Audio device not found: {}", audio_device_name);
        }

        PaStreamParameters output_params;
        output_params.device = *device_index;
        output_params.channelCount = channel_count;
        output_params.sampleFormat = sample_format;
        output_params.suggestedLatency = Pa_GetDeviceInfo(*device_index)->defaultLowOutputLatency;
        output_params.hostApiSpecificStreamInfo = nullptr;

        auto error =
            Pa_OpenStream(&stream_, nullptr, &output_params, sample_rate, k_block_size, paNoFlag, callback, user_data);

        if (error != paNoError) {
            RAV_THROW_EXCEPTION("PortAudio failed to open stream! Error: {}", Pa_GetErrorText(error));
        }

        error = Pa_StartStream(stream_);
        if (error != paNoError) {
            RAV_THROW_EXCEPTION("PortAudio failed to start stream! Error: {}", Pa_GetErrorText(error));
        }

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

    static void print_devices() {
        portaudio::init();
        iterate_devices([](PaDeviceIndex index, const PaDeviceInfo& info) {
            RAV_INFO("[{}]: {}", index, info.name);
            return true;
        });
    }

  private:
    PaStream* stream_ = nullptr;

    static std::optional<PaDeviceIndex> find_device_index_for_name(const std::string& device_name) {
        PaDeviceIndex device_index = -1;

        iterate_devices([&device_name, &device_index](const PaDeviceIndex index, const PaDeviceInfo& device) {
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

    static void iterate_devices(const std::function<bool(PaDeviceIndex index, const PaDeviceInfo&)>& callback) {
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
};

class ravenna_receiver: public rav::RavennaReceiver::Subscriber {
  public:
    explicit ravenna_receiver(
        const std::string& stream_name, std::string audio_device_name, const std::string& interface_address
    ) :
        audio_device_name_(std::move(audio_device_name)) {
        rtsp_client_ = std::make_unique<rav::RavennaRtspClient>(io_context_, browser_);

        rtp_receiver_ = std::make_unique<rav::rtp::Receiver>(io_context_);
        rtp_receiver_->set_interface(asio::ip::make_address(interface_address));

        rav::RavennaReceiver::ConfigurationUpdate update;
        update.delay_frames = 480;  // 10ms at 48KHz
        update.enabled = true;
        update.session_name = stream_name;

        ravenna_receiver_ = std::make_unique<rav::RavennaReceiver>(*rtsp_client_, *rtp_receiver_);
        auto result = ravenna_receiver_->update_configuration(update);
        if (!result) {
            RAV_ERROR("Failed to update configuration: {}", result.error());
            return;
        }
        if (!ravenna_receiver_->subscribe(this)) {
            RAV_WARNING("Failed to add subscriber");
        }
    }

    ~ravenna_receiver() override {
        if (ravenna_receiver_) {
            if (!ravenna_receiver_->unsubscribe(this)) {
                RAV_WARNING("Failed to remove subscriber");
            }
        }
    }

    void run() {
        io_context_.run();
    }

    void stop() {
        io_context_.stop();
        portaudio_stream_.stop();
    }

    void ravenna_receiver_stream_updated(const rav::RavennaReceiver::StreamParameters& event) override {
        if (!event.audio_format.is_valid() || audio_format_ == event.audio_format) {
            return;
        }

        audio_format_ = event.audio_format;
        const auto sample_format = get_sample_format_for_audio_format(audio_format_);
        if (!sample_format.has_value()) {
            RAV_TRACE("Skipping stream update because audio format is invalid: {}", audio_format_.to_string());
            return;
        }
        portaudio_stream_.open_output_stream(
            audio_device_name_, audio_format_.sample_rate, static_cast<int>(audio_format_.num_channels), *sample_format,
            &ravenna_receiver::stream_callback, this
        );
    }

    void on_data_ready([[maybe_unused]] rav::WrappingUint32 timestamp) override {}

  private:
    asio::io_context io_context_;
    rav::RavennaBrowser browser_ {io_context_};
    std::unique_ptr<rav::RavennaRtspClient> rtsp_client_;
    std::unique_ptr<rav::rtp::Receiver> rtp_receiver_;
    std::unique_ptr<rav::RavennaReceiver> ravenna_receiver_;
    std::string audio_device_name_;
    portaudio_stream portaudio_stream_;
    rav::AudioFormat audio_format_;

    int stream_callback(
        const void* input, void* output, const unsigned long frame_count, const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags status_flags
    ) {
        TRACY_ZONE_SCOPED;

        std::ignore = input;
        std::ignore = time_info;
        std::ignore = status_flags;

        const auto buffer_size = frame_count * audio_format_.bytes_per_frame();

        if (!ravenna_receiver_->read_data_realtime(static_cast<uint8_t*>(output), buffer_size, {})) {
            std::memset(output, audio_format_.ground_value(), buffer_size);
            RAV_WARNING("Failed to read data");
            return paContinue;
        }

        if (audio_format_.byte_order == rav::AudioFormat::ByteOrder::be) {
            rav::swap_bytes(static_cast<uint8_t*>(output), buffer_size, audio_format_.bytes_per_sample());
        }

        return paContinue;
    }

    static int stream_callback(
        const void* input, void* output, const unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo,
        const PaStreamCallbackFlags statusFlags, void* userData
    ) {
        return static_cast<ravenna_receiver*>(userData)->stream_callback(
            input, output, frameCount, timeInfo, statusFlags
        );
    }

    static std::optional<uint32_t> get_sample_format_for_audio_format(const rav::AudioFormat& audio_format) {
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
};

}  // namespace examples

/**
 * This examples demonstrates how to receive audio streams from a RAVENNA device. It sets up a RAVENNA sink that listens
 * for announcements from a RAVENNA device and starts receiving audio data. It will play the audio to the selected audio
 * device using portaudio.
 * Warning! No drift correction is done between the sender and receiver. At some point buffers will overflow or
 * underflow.
 */
int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::string stream_name;
    app.add_option("stream_name", stream_name, "The name of the stream to receive")->required();

    std::string audio_output_device;
    app.add_option("audio_output_device", audio_output_device, "The name of the audio output device")->required();

    std::string interface_address = "0.0.0.0";
    app.add_option("--interface-addr", interface_address, "The interface address");

    CLI11_PARSE(app, argc, argv);

    examples::portaudio_stream::print_devices();

    examples::ravenna_receiver receiver_example(stream_name, audio_output_device, interface_address);

    std::thread cin_thread([&receiver_example] {
        fmt::println("Press return key to stop...");
        std::cin.get();
        receiver_example.stop();
    });

    receiver_example.run();
    cin_thread.join();

    return 0;
}
