/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ravenna/ravenna_receiver.hpp"
#include "ravennakit/ravenna/ravenna_sender.hpp"
#include "ravennakit/rtp/detail/rtp_sender.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"

#include <CLI/App.hpp>

namespace examples {
class loopback: public rav::rtp::StreamReceiver::Subscriber {
  public:
    explicit loopback(std::string stream_name, const asio::ip::address_v4& interface_addr) :
        stream_name_(std::move(stream_name)) {
        rtsp_client_ = std::make_unique<rav::RavennaRtspClient>(io_context_, browser_);
        auto config = rav::rtp::Receiver::Configuration {interface_addr};
        rtp_receiver_ = std::make_unique<rav::rtp::Receiver>(io_context_, config);

        ravenna_receiver_ = std::make_unique<rav::RavennaReceiver>(*rtsp_client_, *rtp_receiver_);
        ravenna_receiver_->set_delay(480);  // 10ms @ 48kHz
        if (!ravenna_receiver_->subscribe(this)) {
            RAV_WARNING("Failed to add subscriber");
        }
        std::ignore = ravenna_receiver_->subscribe_to_session(stream_name_);

        advertiser_ = rav::dnssd::Advertiser::create(io_context_);
        if (advertiser_ == nullptr) {
            RAV_THROW_EXCEPTION("No dnssd advertiser available");
        }

        rtsp_server_ = std::make_unique<rav::rtsp::Server>(
            io_context_, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 5005)
        );

        rtp_sender_ = std::make_unique<rav::rtp::Sender>(io_context_, interface_addr);

        ptp_instance_ = std::make_unique<rav::ptp::Instance>(io_context_);
        if (const auto result = ptp_instance_->add_port(interface_addr); !result) {
            RAV_THROW_EXCEPTION("Failed to add PTP port: {}", to_string(result.error()));
        }

        ptp_port_changed_event_slot_ = ptp_instance_->on_port_changed_state.subscribe([this](auto event) {
            if (event.port.state() == rav::ptp::State::slave) {
                RAV_INFO("Port state changed to slave, start playing");
                ptp_clock_stable_ = true;
                start_transmitting();  // Also called when the first packet is received
            }
        });

        sender_ = std::make_unique<rav::RavennaSender>(
            io_context_, *advertiser_, *rtsp_server_, *ptp_instance_, *rtp_sender_, rav::Id(1),
            stream_name_ + "_loopback", interface_addr
        );

        sender_->on<rav::RavennaSender::OnDataRequestedEvent>([this](auto event) {
            std::ignore = ravenna_receiver_->read_data_realtime(
                event.buffer.data(), event.buffer.size(), event.timestamp - ravenna_receiver_->get_delay()
            );
        });
    }

    ~loopback() override {
        if (ravenna_receiver_ != nullptr) {
            if (!ravenna_receiver_->unsubscribe(this)) {
                RAV_WARNING("Failed to remove subscriber");
            }
        }
    }

    void rtp_stream_receiver_updated(const rav::rtp::StreamReceiver::StreamUpdatedEvent& event) override {
        buffer_.resize(event.selected_audio_format.bytes_per_frame() * event.packet_time_frames);
        if (!sender_->set_audio_format(event.selected_audio_format)) {
            RAV_ERROR("Format not supported by transmitter");
            return;
        }
        start_transmitting();
    }

    void on_data_ready(rav::WrappingUint32 timestamp) override {
        most_recent_timestamp_ = timestamp;
    }

    void run() {
        io_context_.run();
    }

  private:
    std::string stream_name_;
    asio::io_context io_context_;
    std::vector<uint8_t> buffer_;
    std::optional<rav::WrappingUint32> most_recent_timestamp_;
    bool ptp_clock_stable_ = false;

    // Receiver components
    rav::RavennaBrowser browser_ {io_context_};
    std::unique_ptr<rav::RavennaRtspClient> rtsp_client_;
    std::unique_ptr<rav::rtp::Receiver> rtp_receiver_;
    std::unique_ptr<rav::RavennaReceiver> ravenna_receiver_;

    // Sender components
    std::unique_ptr<rav::dnssd::Advertiser> advertiser_;
    std::unique_ptr<rav::rtsp::Server> rtsp_server_;
    std::unique_ptr<rav::rtp::Sender> rtp_sender_;
    std::unique_ptr<rav::ptp::Instance> ptp_instance_;
    std::unique_ptr<rav::RavennaSender> sender_;
    rav::EventSlot<rav::ptp::Instance::PortChangedStateEventEvent> ptp_port_changed_event_slot_;

    /**
     * Starts transmitting if the PTP clock is stable and a timestamp is available and transmitter is not already
     * running.
     */
    void start_transmitting() const {
        if (ptp_clock_stable_ && most_recent_timestamp_) {
            sender_->start(most_recent_timestamp_->value());
        }
    }
};

}  // namespace examples

/**
 * This example subscribes to a RAVENNA stream, reads the audio data, and writes it back to the network as a different
 * stream. The purpose of this example is to show (and test) how low the latency can be when using the RAVENNA API and
 * to demonstrate how the playback is sample-sync (although you'll have to measure than on a real device).
 */

int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Loopback example"};
    argv = app.ensure_utf8(argv);

    std::string stream_name;
    app.add_option("stream_name", stream_name, "The name of the stream to loop back")->required();

    std::string interface_address_string = "0.0.0.0";
    app.add_option("--interface-addr", interface_address_string, "The interface address");

    CLI11_PARSE(app, argc, argv);

    const auto interface_address = asio::ip::make_address_v4(interface_address_string);

    // Receiving side
    examples::loopback example(stream_name, interface_address);
    example.run();

    return 0;
}
