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
class loopback: public rav::RavennaReceiver::Subscriber, public rav::ptp::Instance::Subscriber {
  public:
    explicit loopback(std::string stream_name, const std::string& interface_search_string) :
        stream_name_(std::move(stream_name)) {
        rtsp_client_ = std::make_unique<rav::RavennaRtspClient>(io_context_, browser_);
        advertiser_ = rav::dnssd::Advertiser::create(io_context_);
        if (advertiser_ == nullptr) {
            RAV_THROW_EXCEPTION("No dnssd advertiser available");
        }

        rtsp_server_ = std::make_unique<rav::rtsp::Server>(
            io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), 5005)
        );

        const auto* iface = rav::NetworkInterfaceList::get_system_interfaces().find_by_string(interface_search_string);
        if (iface == nullptr) {
            RAV_ERROR("No network interface found with search string: {}", interface_search_string);
            return;
        }

        ptp_instance_ = std::make_unique<rav::ptp::Instance>(io_context_);
        if (const auto result = ptp_instance_->add_port(iface->get_first_ipv4_address()); !result) {
            RAV_THROW_EXCEPTION("Failed to add PTP port: {}", to_string(result.error()));
        }

        if (!ptp_instance_->subscribe(this)) {
            RAV_WARNING("Failed to add subscriber");
        }

        sender_ = std::make_unique<rav::RavennaSender>(
            io_context_, *advertiser_, *rtsp_server_, *ptp_instance_, rav::Id::get_next_process_wide_unique_id(), 1
        );

        rav::NetworkInterfaceConfig interface_config;
        interface_config.set_interface(rav::Rank::primary(), iface->get_identifier());

        sender_->set_network_interface_config(std::move(interface_config));

        rtp_receiver_ = std::make_unique<rav::rtp::Receiver3>(io_context_);

        rav::RavennaReceiver::Configuration config;
        config.delay_frames = 480;  // 10ms at 48KHz
        config.enabled = true;
        config.session_name = stream_name_;

        ravenna_receiver_ = std::make_unique<rav::RavennaReceiver>(
            io_context_, *rtsp_client_, *rtp_receiver_, rav::Id::get_next_process_wide_unique_id()
        );
        ravenna_receiver_->set_network_interface_config(std::move(interface_config));
        auto result = ravenna_receiver_->set_configuration(config);
        if (!result) {
            RAV_ERROR("Failed to update configuration: {}", result.error());
            return;
        }
    }

    ~loopback() override {
        if (ptp_instance_ != nullptr) {
            if (!ptp_instance_->unsubscribe(this)) {
                RAV_WARNING("Failed to remove subscriber");
            }
        }

        if (ravenna_receiver_ != nullptr) {
            if (!ravenna_receiver_->unsubscribe(this)) {
                RAV_WARNING("Failed to remove subscriber");
            }
        }
    }

    void ravenna_receiver_parameters_updated(const rav::rtp::Receiver3::ReaderParameters& parameters) override {
        if (parameters.streams.empty()) {
            RAV_WARNING("No streams available");
            return;
        }

        RAV_ASSERT(parameters.audio_format.is_valid(), "Invalid audio format");

        buffer_.resize(parameters.audio_format.bytes_per_frame() * parameters.streams.front().packet_time_frames);

        rav::RavennaSender::Configuration config;
        config.session_name = stream_name_ + "_loopback";
        config.audio_format = parameters.audio_format;
        config.enabled = true;

        auto result = sender_->set_configuration(config);
        if (!result) {
            RAV_ERROR("Failed to update sender: {}", result.error());
        }
    }

    void on_data_ready(rav::WrappingUint32 timestamp) override {
        if (!ravenna_receiver_->read_data_realtime(buffer_.data(), buffer_.size(), timestamp.value())) {
            RAV_ERROR("Failed to read data from receiver");
            return;
        }

        timestamp += ravenna_receiver_->get_configuration().delay_frames;
        std::ignore = sender_->send_data_realtime(rav::BufferView(buffer_).const_view(), timestamp.value());
    }

    void ptp_port_changed_state(const rav::ptp::Port& port) override {
        if (port.state() == rav::ptp::State::slave) {
            RAV_INFO("Port state changed to slave, start playing");
            ptp_clock_stable_ = true;
        }
    }

    void run() {
        while (!io_context_.stopped()) {
            io_context_.poll();
        }
    }

  private:
    std::string stream_name_;
    boost::asio::io_context io_context_;
    std::vector<uint8_t> buffer_;
    bool ptp_clock_stable_ = false;

    // Receiver components
    rav::RavennaBrowser browser_ {io_context_};
    std::unique_ptr<rav::RavennaRtspClient> rtsp_client_;
    std::unique_ptr<rav::rtp::Receiver3> rtp_receiver_;
    std::unique_ptr<rav::RavennaReceiver> ravenna_receiver_;

    // Sender components
    std::unique_ptr<rav::dnssd::Advertiser> advertiser_;
    std::unique_ptr<rav::rtsp::Server> rtsp_server_;
    std::unique_ptr<rav::ptp::Instance> ptp_instance_;
    std::unique_ptr<rav::RavennaSender> sender_;
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

    // Receiving side
    examples::loopback example(stream_name, interface_address_string);
    example.run();

    return 0;
}
