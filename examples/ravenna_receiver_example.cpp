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
#include "ravennakit/ravenna/ravenna_sink.hpp"

#include <portaudio.h>
#include <CLI/App.hpp>
#include <asio/io_context.hpp>
#include <utility>

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

/**
 * This examples demonstrates how to receive audio streams from a RAVENNA device. It sets up a RAVENNA sink that listens
 * for announcements from a RAVENNA device and starts receiving audio data. It will play the audio to the selected audio
 * device using portaudio.
 */
class ravenna_receiver_example: public rav::rtp_stream_receiver::subscriber {
  public:
    explicit ravenna_receiver_example(
        const std::string& stream_name, std::string audio_device_name, const std::string& interface_address
    ) :
        audio_device_name_(std::move(audio_device_name)) {
        node_browser_ = rav::dnssd::dnssd_browser::create(io_context_);

        if (node_browser_ == nullptr) {
            RAV_THROW_EXCEPTION("No dnssd browser available");
        }

        node_browser_->browse_for("_rtsp._tcp,_ravenna_session");

        rtsp_client_ = std::make_unique<rav::ravenna_rtsp_client>(io_context_, *node_browser_);

        rav::rtp_receiver::configuration config;
        config.interface_address = asio::ip::make_address(interface_address);
        rtp_receiver_ = std::make_unique<rav::rtp_receiver>(io_context_, config);

        ravenna_sink_ = std::make_unique<rav::ravenna_sink>(*rtsp_client_, *rtp_receiver_, stream_name);
        ravenna_sink_->add_subscriber(this);
    }

    ~ravenna_receiver_example() override {
        if (ravenna_sink_) {
            ravenna_sink_->remove_subscriber(this);
        }
    }

    void start() {
        io_context_.run();
    }

    void stop() {
        io_context_.stop();
        portaudio_stream_.stop();
    }

    void on_audio_format_changed(const rav::audio_format& new_format) override {
        audio_format_ = new_format;
        const auto sample_format = get_sample_format_for_audio_format(new_format);
        if (!sample_format.has_value()) {
            RAV_ERROR("Unsupported audio format: {}", new_format.to_string());
            return;
        }
        portaudio_stream_.open_output_stream(
            audio_device_name_, new_format.sample_rate, static_cast<int>(new_format.num_channels), *sample_format,
            &ravenna_receiver_example::stream_callback, this
        );
        timestamp_ = std::nullopt;
    }

    void on_stream_started() override {}

    void on_first_packet(const uint32_t timestamp, const uint32_t delay) override {
        timestamp_ = timestamp - delay;
        RAV_TRACE("First packet received with timestamp: {}", timestamp);
    }

  private:
    asio::io_context io_context_;
    std::unique_ptr<rav::dnssd::dnssd_browser> node_browser_;
    std::unique_ptr<rav::ravenna_rtsp_client> rtsp_client_;
    std::unique_ptr<rav::rtp_receiver> rtp_receiver_;
    std::unique_ptr<rav::ravenna_sink> ravenna_sink_;
    std::string audio_device_name_;
    portaudio_stream portaudio_stream_;
    rav::audio_format audio_format_;
    std::optional<uint32_t> timestamp_ {};

    int stream_callback(
        const void* input, void* output, const unsigned long frame_count, const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags status_flags
    ) {
        TRACY_ZONE_SCOPED;

        std::ignore = input;
        std::ignore = time_info;
        std::ignore = status_flags;

        const auto buffer_size = frame_count * audio_format_.bytes_per_frame();

        if (!timestamp_.has_value()) {
            std::memset(output, audio_format_.ground_value(), buffer_size);
            return paContinue;
        }

        if (!ravenna_sink_->read_data(*timestamp_, static_cast<uint8_t*>(output), buffer_size)) {
            std::memset(output, audio_format_.ground_value(), buffer_size);
        }
        *timestamp_ += frame_count;

        if (audio_format_.byte_order == rav::audio_format::byte_order::be) {
            rav::byte_order::swap_bytes(static_cast<uint8_t*>(output), buffer_size, audio_format_.bytes_per_sample());
        }

        TRACY_PLOT("Audio timestamp", static_cast<int64_t>(*timestamp_));

        return paContinue;
    }

    static int stream_callback(
        const void* input, void* output, const unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo,
        const PaStreamCallbackFlags statusFlags, void* userData
    ) {
        return static_cast<ravenna_receiver_example*>(userData)->stream_callback(
            input, output, frameCount, timeInfo, statusFlags
        );
    }

    static std::optional<uint32_t> get_sample_format_for_audio_format(const rav::audio_format& audio_format) {
        std::array<std::pair<rav::audio_encoding, uint32_t>, 5> pairs {
            {{rav::audio_encoding::pcm_u8, paUInt8},
             {rav::audio_encoding::pcm_s8, paInt8},
             {rav::audio_encoding::pcm_s16, paInt16},
             {rav::audio_encoding::pcm_s24, paInt24},
             {rav::audio_encoding::pcm_s32, paInt32}},
        };
        for (auto& pair : pairs) {
            if (pair.first == audio_format.encoding) {
                return pair.second;
            }
        }
        return std::nullopt;
    }
};

int main(int const argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::string stream_name;
    app.add_option("stream_name", stream_name, "The name of the stream to receive")->required();

    std::string audio_output_device;
    app.add_option("audio_output_device", audio_output_device, "The name of the audio output device")->required();

    std::string interface_address = "0.0.0.0";
    app.add_option("--interface-addr", interface_address, "The interface address");

    CLI11_PARSE(app, argc, argv);

    portaudio_stream::print_devices();

    ravenna_receiver_example receiver_example(stream_name, audio_output_device, interface_address);

    std::thread cin_thread([&receiver_example] {
        fmt::println("Press return key to stop...");
        std::cin.get();
        receiver_example.stop();
    });

    receiver_example.start();
    cin_thread.join();

    return 0;
}
