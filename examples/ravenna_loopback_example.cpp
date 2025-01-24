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
 * This example subscribes to a RAVENNA stream, reads the audio data, and writes it back to the network as a different
 * stream. The purpose of this example is to show (and test) how low the latency can be when using the RAVENNA API and
 * to demonstrate how the playback is sample-sync (although you'll have to measure than on a real device).
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ravenna/ravenna_receiver.hpp"
#include "ravennakit/ravenna/ravenna_transmitter.hpp"
#include "ravennakit/rtp/detail/rtp_transmitter.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"

#include <CLI/App.hpp>

class loopback_example: public rav::rtp_stream_receiver::subscriber {
  public:
    explicit loopback_example(std::string stream_name, const asio::ip::address_v4& interface_addr) :
        stream_name_(std::move(stream_name)) {
        browser_ = rav::dnssd::dnssd_browser::create(io_context_);
        if (browser_ == nullptr) {
            RAV_THROW_EXCEPTION("No dnssd browser available");
        }
        browser_->browse_for("_rtsp._tcp,_ravenna_session");

        rtsp_client_ = std::make_unique<rav::ravenna_rtsp_client>(io_context_, *browser_);
        auto config = rav::rtp_receiver::configuration {interface_addr};
        rtp_receiver_ = std::make_unique<rav::rtp_receiver>(io_context_, config);

        ravenna_receiver_ = std::make_unique<rav::ravenna_receiver>(*rtsp_client_, *rtp_receiver_, stream_name_);
        ravenna_receiver_->set_delay(480); // 10ms @ 48kHz
        ravenna_receiver_->add_subscriber(this);

        advertiser_ = rav::dnssd::dnssd_advertiser::create(io_context_);
        if (advertiser_ == nullptr) {
            RAV_THROW_EXCEPTION("No dnssd advertiser available");
        }

        rtsp_server_ =
            std::make_unique<rav::rtsp_server>(io_context_, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 5005));

        rtp_transmitter_ = std::make_unique<rav::rtp_transmitter>(io_context_, interface_addr);

        ptp_instance_ = std::make_unique<rav::ptp_instance>(io_context_);
        if (const auto result = ptp_instance_->add_port(interface_addr); !result) {
            RAV_THROW_EXCEPTION("Failed to add PTP port: {}", to_string(result.error()));
        }

        ptp_slot_ = ptp_instance_->on_port_changed_state.subscribe([this](auto event) {
            if (event.port.state() == rav::ptp_state::slave) {
                RAV_INFO("Port state changed to slave, start playing");
                ptp_clock_stable_ = true;
                start_transmitter();
            }
        });

        transmitter_ = std::make_unique<rav::ravenna_transmitter>(
            io_context_, *advertiser_, *rtsp_server_, *ptp_instance_, *rtp_transmitter_, rav::id(1),
            stream_name_ + "_loopback", interface_addr
        );

        transmitter_->on<rav::ravenna_transmitter::on_data_requested_event>([this](auto event) {
            ravenna_receiver_->read_data(
                event.timestamp - ravenna_receiver_->get_delay(), event.buffer.data(), event.buffer.size()
            );
        });
    }

    ~loopback_example() override {
        ravenna_receiver_->remove_subscriber(this);
    }

    void on_audio_format_changed(const rav::audio_format& new_format, const uint32_t packet_time_frames) override {
        buffer_.resize(new_format.bytes_per_frame() * packet_time_frames);
        if (!transmitter_->set_audio_format(new_format)) {
            RAV_ERROR("Format not supported by transmitter");
        }
    }

    void on_data_available(const uint32_t timestamp) override {
        if (!start_streaming_at_) {
            start_streaming_at_ = timestamp + ravenna_receiver_->get_delay();
            start_transmitter();
        }
    }

    void run() {
        io_context_.run();
    }

  private:
    std::string stream_name_;
    asio::io_context io_context_;
    std::vector<uint8_t> buffer_;
    std::optional<uint32_t> start_streaming_at_;
    bool ptp_clock_stable_ = false;

    // Receiver components
    std::unique_ptr<rav::dnssd::dnssd_browser> browser_;
    std::unique_ptr<rav::ravenna_rtsp_client> rtsp_client_;
    std::unique_ptr<rav::rtp_receiver> rtp_receiver_;
    std::unique_ptr<rav::ravenna_receiver> ravenna_receiver_;

    // Sender components
    std::unique_ptr<rav::dnssd::dnssd_advertiser> advertiser_;
    std::unique_ptr<rav::rtsp_server> rtsp_server_;
    std::unique_ptr<rav::rtp_transmitter> rtp_transmitter_;
    std::unique_ptr<rav::ptp_instance> ptp_instance_;
    std::unique_ptr<rav::ravenna_transmitter> transmitter_;
    rav::event_slot<rav::ptp_instance::port_changed_state_event> ptp_slot_;

    void start_transmitter() {
        if (ptp_clock_stable_ && start_streaming_at_) {
            transmitter_->start(*start_streaming_at_);
        }
    }
};

int main(int const argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    CLI::App app {"RAVENNA Loopback example"};
    argv = app.ensure_utf8(argv);

    std::string stream_name;
    app.add_option("stream_name", stream_name, "The name of the stream to loop back")->required();

    std::string interface_address_string = "0.0.0.0";
    app.add_option("--interface-addr", interface_address_string, "The interface address");

    CLI11_PARSE(app, argc, argv);

    const auto interface_address = asio::ip::make_address_v4(interface_address_string);

    // Receiving side
    loopback_example example(stream_name, interface_address);
    example.run();

    return 0;
}
