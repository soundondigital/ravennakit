/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

/**
 * This examples demonstrates how to create a source and send audio onto the network. It does this by reading audio from
 * a wav file on disk and sending it as multicast audio packets.
 */

#include "ravennakit/core/file.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/core/audio/formats/wav_audio_format.hpp"
#include "ravennakit/core/streams/file_input_stream.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ravenna/ravenna_transmitter.hpp"

#include <CLI/App.hpp>
#include <asio/io_context.hpp>
#include <utility>

/**
 * Holds the logic for transmitting a wav file over the network.
 */
class wav_file_player {
  public:
    explicit wav_file_player(
        asio::io_context& io_context, rav::dnssd::dnssd_advertiser& advertiser, rav::rtsp_server& rtsp_server,
        rav::ptp_instance& ptp_instance, rav::rtp_transmitter& rtp_transmitter, rav::id::generator& id_generator,
        const asio::ip::address_v4& interface_address, const rav::file& file_to_play, const std::string& session_name
    ) {
        if (!file_to_play.exists()) {
            throw std::runtime_error("File does not exist: " + file_to_play.path().string());
        }

        auto transmitter = std::make_unique<rav::ravenna_transmitter>(
            io_context, advertiser, rtsp_server, ptp_instance, rtp_transmitter, id_generator.next(), session_name,
            interface_address
        );

        auto file_input_stream = std::make_unique<rav::file_input_stream>(file_to_play);
        auto reader = std::make_unique<rav::wav_audio_format::reader>(std::move(file_input_stream));

        const auto format = reader->get_audio_format();
        if (!format) {
            throw std::runtime_error("Failed to read audio format from file: " + file_to_play.path().string());
        }

        if (!transmitter->set_audio_format(*format)) {
            throw std::runtime_error("Unsupported audio format for transmitter: " + file_to_play.path().string());
        }

        reader_ = std::move(reader);
        transmitter_ = std::move(transmitter);

        transmitter_->on<rav::ravenna_transmitter::on_data_requested_event>([this](auto event) {
            TRACY_ZONE_SCOPED;

            if (reader_->remaining_audio_data() == 0) {
                reader_->set_read_position(0);
            }

            auto result = reader_->read_audio_data(event.buffer.data(), event.buffer.size_bytes());
            if (!result) {
                RAV_ERROR("Failed to read audio data: {}", rav::input_stream::to_string(result.error()));
                return;
            }
            auto read = result.value();
            if (read == 0) {
                RAV_ERROR("No bytes read");
            }
            if (read < event.buffer.size_bytes()) {
                // Clear the part of the buffer where we didn't write any data
                std::fill(event.buffer.data() + read, event.buffer.data() + event.buffer.size_bytes(), 0);
            }
        });
    }

    void start(const rav::ptp_timestamp at) const {
        transmitter_->start(at);
    }

  private:
    std::unique_ptr<rav::wav_audio_format::reader> reader_;
    std::unique_ptr<rav::ravenna_transmitter> transmitter_;
};

int main(int const argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    CLI::App app {"RAVENNA Player example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> file_paths;
    app.add_option("files", file_paths, "The files to stream")->required();

    std::string interface_address_string = "0.0.0.0";
    app.add_option("--interface-addr", interface_address_string, "The interface address");

    CLI11_PARSE(app, argc, argv);

    const auto interface_address = asio::ip::make_address_v4(interface_address_string);

    asio::io_context io_context;

    std::vector<std::unique_ptr<wav_file_player>> wav_file_players;

    auto advertiser = rav::dnssd::dnssd_advertiser::create(io_context);
    rav::rtsp_server rtsp_server(io_context, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 5005));
    rav::rtp_transmitter rtp_transmitter(io_context, interface_address);

    // PTP
    rav::ptp_instance ptp_instance(io_context);
    auto slot = ptp_instance.on_port_changed_state.subscribe([&ptp_instance, &wav_file_players](auto event) {
        if (event.port.state() == rav::ptp_state::slave) {
            RAV_INFO("Port state changed to slave, start players");
            auto start_at = ptp_instance.get_local_ptp_time();
            start_at.add_seconds(0.5);
            for (const auto& player : wav_file_players) {
                player->start(start_at);
            }
        }
    });

    if (const auto result = ptp_instance.add_port(interface_address); !result) {
        RAV_THROW_EXCEPTION("Failed to add PTP port: {}", to_string(result.error()));
    }

    // ID generator
    rav::id::generator id_generator;

    for (auto& file_path : file_paths) {
        auto file = rav::file(file_path);
        const auto file_session_name = file.path().filename().string();

        wav_file_players.emplace_back(
            std::make_unique<wav_file_player>(
                io_context, *advertiser, rtsp_server, ptp_instance, rtp_transmitter, id_generator, interface_address,
                file, file_session_name + " " + std::to_string(wav_file_players.size() + 1)
            )
        );
    }

    io_context.run();

    return 0;
}
