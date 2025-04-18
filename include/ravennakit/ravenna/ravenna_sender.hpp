/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/aes67/aes67_packet_time.hpp"
#include "ravennakit/core/uri.hpp"
#include "ravennakit/core/json.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/containers/fifo_buffer.hpp"
#include "ravennakit/core/sync/rcu.hpp"
#include "ravennakit/core/sync/realtime_shared_object.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ptp/types/ptp_timestamp.hpp"
#include "ravennakit/rtp/rtp_packet.hpp"
#include "ravennakit/rtp/detail/rtp_buffer.hpp"
#include "ravennakit/rtp/detail/rtp_sender.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

class RavennaSender: public rtsp::Server::PathHandler, public ptp::Instance::Subscriber {
  public:
    /// The number of packet buffers available for sending. This value means that n packets worth of data can be queued
    /// for sending.
    static constexpr uint32_t k_buffer_num_packets = 30;

    /// The max number of frames to feed into the sender (using send_audio_data_realtime). This will usually correspond
    /// to an audio device buffer size.
    static constexpr uint32_t k_max_num_frames = 4096;

    /**
     * Defines the configuration for the sender.
     */
    struct Configuration {
        std::string session_name;
        asio::ip::address_v4 destination_address;
        int32_t ttl {};
        uint8_t payload_type {};
        AudioFormat audio_format;
        aes67::PacketTime packet_time;
        bool enabled {};

        /**
         * @return The configuration as a JSON object.
         */
        [[nodiscard]] nlohmann::json to_json() const;
    };

    /**
     * Struct for updating the configuration of the sender. Only the fields that are set are taken into account, which
     * allows for partial updates.
     */
    struct ConfigurationUpdate {
        std::optional<std::string> session_name;
        std::optional<asio::ip::address_v4> destination_address;
        std::optional<int32_t> ttl;
        std::optional<uint8_t> payload_type;
        std::optional<AudioFormat> audio_format;
        std::optional<aes67::PacketTime> packet_time;
        std::optional<bool> enabled;

        /**
         * Creates a configuration changes object from a JSON object.
         * @param json The JSON object to convert.
         * @return A configuration update object if the JSON is valid, otherwise an error message.
         */
        static tl::expected<ConfigurationUpdate, std::string> from_json(const nlohmann::json& json);
    };

    class Subscriber {
      public:
        virtual ~Subscriber() = default;

        virtual void ravenna_sender_configuration_updated(const Id sender_id, const Configuration& configuration) {
            std::ignore = sender_id;
            std::ignore = configuration;
        }
    };

    RavennaSender(
        asio::io_context& io_context, dnssd::Advertiser& advertiser, rtsp::Server& rtsp_server,
        ptp::Instance& ptp_instance, Id id, uint32_t session_id, const asio::ip::address_v4& interface_address,
        ConfigurationUpdate initial_config = {}
    );

    ~RavennaSender() override;

    RavennaSender(const RavennaSender& other) = delete;
    RavennaSender& operator=(const RavennaSender& other) = delete;

    RavennaSender(RavennaSender&& other) noexcept = delete;
    RavennaSender& operator=(RavennaSender&& other) noexcept = delete;

    /**
     * @return The sender ID.
     */
    [[nodiscard]] Id get_id() const;

    /**
     * @return The session ID of the sender.
     */
    [[nodiscard]] uint32_t get_session_id() const {
        return session_id_;
    }

    /**
     * Updates the configuration of the sender. Only takes into account the fields in the configuration that are set.
     * This allows to update only a subset of the configuration.
     * @param update The configuration to update.
     */
    [[nodiscard]] tl::expected<void, std::string> set_configuration(const ConfigurationUpdate& update);

    /**
     * @returns The current configuration of the sender.
     */
    [[nodiscard]] const Configuration& get_configuration() const;

    /**
     * Subscribes to the sender.
     * @param subscriber The subscriber to subscribe.
     * @return True if the subscriber was successfully subscribed, false otherwise.
     */
    [[nodiscard]] bool subscribe(Subscriber* subscriber);

    /**
     * Unsubscribes from the sender.
     * @param subscriber The subscriber to unsubscribe.
     * @return
     */
    [[nodiscard]] bool unsubscribe(Subscriber* subscriber);

    /**
     * @return The packet time in milliseconds as signaled using SDP. If the packet time is 1ms and the sample
     * rate is 44.1kHz, then the signaled packet time is 1.09.
     */
    [[nodiscard]] float get_signaled_ptime() const;

    /**
     * @return The packet size in number of frames.
     */
    [[nodiscard]] uint32_t get_framecount() const;

    /**
     * Schedules data for sending. A call to this function is realtime safe and thread safe as long as only one thread
     * makes the call.
     * @param buffer The buffer to send.
     * @param timestamp The timestamp of the buffer.
     * @returns True if the buffer was sent, or false if something went wrong.
     */
    [[nodiscard]] bool send_data_realtime(BufferView<const uint8_t> buffer, uint32_t timestamp);

    /**
     * Schedules audio data for sending. A call to this function is realtime safe and thread safe as long as only one
     * thread makes the call.
     * @param input_buffer The buffer to send.
     * @param timestamp The timestamp of the buffer.
     * @return True if the buffer was sent, or false if something went wrong.
     */
    [[nodiscard]] bool send_audio_data_realtime(const AudioBufferView<const float>& input_buffer, uint32_t timestamp);

    /**
     * Sets the interface to use for this sender.
     * @param interface_address The address of the interface to use.
     */
    void set_interface(const asio::ip::address_v4& interface_address);

    /**
     * @return A JSON representation of the sender.
     */
    nlohmann::json to_json() const;

    // rtsp_server::handler overrides
    void on_request(rtsp::Connection::RequestEvent event) const override;

    // ptp::Instance::Subscriber overrides
    void ptp_parent_changed(const ptp::ParentDs& parent) override;
    void ptp_port_changed_state(const ptp::Port& port) override;

  private:
    [[maybe_unused]] asio::io_context& io_context_;
    dnssd::Advertiser& advertiser_;
    rtsp::Server& rtsp_server_;
    ptp::Instance& ptp_instance_;

    Id id_;
    uint32_t session_id_ {};
    Configuration configuration_;
    std::string rtsp_path_by_name_;
    std::string rtsp_path_by_id_;
    Id advertisement_id_;
    int32_t clock_domain_ {};
    ptp::ClockIdentity grandmaster_identity_;
    rtp::Sender rtp_sender_;

    asio::high_resolution_timer timer_;
    SubscriberList<Subscriber> subscribers_;
    std::atomic<bool> ptp_stable_ {false};

    struct Packet {
        uint32_t rtp_timestamp {};
        uint32_t payload_size_bytes {};
        std::array<uint8_t, aes67::constants::k_max_payload> payload {};
    };

    struct SharedContext {
        // Network thread:
        asio::ip::udp::endpoint destination_endpoint;

        // Audio thread:
        ByteBuffer rtp_packet_buffer;
        std::vector<uint8_t> intermediate_send_buffer;
        std::vector<uint8_t> intermediate_audio_buffer;
        uint32_t packet_time_frames;
        rtp::Packet rtp_packet;
        AudioFormat audio_format;
        rtp::Buffer rtp_buffer;

        // Audio thread writes and network thread reads:
        FifoBuffer<Packet, Fifo::Spsc> outgoing_data;
    };

    Rcu<SharedContext> shared_context_;
    Rcu<SharedContext>::Reader send_data_realtime_reader_ {shared_context_.create_reader()};
    Rcu<SharedContext>::Reader send_outgoing_data_reader_ {shared_context_.create_reader()};

    /**
     * Sends an announce request to all connected clients.
     */
    void send_announce() const;
    [[nodiscard]] sdp::SessionDescription build_sdp() const;
    void start_timer();
    void stop_timer();
    void send_outgoing_data();
    void update_shared_context();
};

}  // namespace rav
