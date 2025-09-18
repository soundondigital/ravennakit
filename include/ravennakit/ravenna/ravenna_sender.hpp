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
#include "ravennakit/core/util/uri.hpp"
#include "ravennakit/core/json.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/containers/fifo_buffer.hpp"
#include "ravennakit/core/sync/rcu.hpp"
#include "ravennakit/core/util/rank.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/nmos/nmos_node.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/rtp/rtp_packet.hpp"
#include "ravennakit/rtp/detail/rtp_ringbuffer.hpp"
#include "ravennakit/rtp/detail/rtp_sender.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>

namespace rav {

class RavennaSender: public rtsp::Server::PathHandler, public ptp::Instance::Subscriber {
  public:
    /// The number of packet buffers available for sending. This value means that n packets worth of data can be queued
    /// for sending.
    static constexpr uint32_t k_buffer_num_packets = 30;

    /// The max number of frames to feed into the sender (using send_audio_data_realtime). This will usually correspond
    /// to an audio device buffer size.
    static constexpr uint32_t k_max_num_frames = 4096;

    /// List of supported audio encodings for the sender.
    static constexpr auto k_supported_encodings = {AudioEncoding::pcm_s16, AudioEncoding::pcm_s24};

    /**
     * The destination of where a stream of packets should go. The sender can send to multiple destinations, but each
     * destination can only be used by one sender. The destination is defined by a port and an address.
     */
    struct Destination {
        Rank interface_by_rank;
        boost::asio::ip::udp::endpoint endpoint {};
        bool enabled {};

        friend bool operator==(const Destination& lhs, const Destination& rhs) {
            return std::tie(lhs.interface_by_rank, lhs.endpoint, lhs.enabled)
                == std::tie(rhs.interface_by_rank, rhs.endpoint, rhs.enabled);
        }

        friend bool operator!=(const Destination& lhs, const Destination& rhs) {
            return !(lhs == rhs);
        }
    };

    /*
     * Defines the configuration for the sender.
     */
    struct Configuration {
        std::string session_name;
        std::vector<Destination> destinations;
        int32_t ttl {};
        uint8_t payload_type {};
        AudioFormat audio_format;
        aes67::PacketTime packet_time;
        bool enabled {};
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
        rtp::AudioSender& rtp_audio_sender, dnssd::Advertiser& advertiser, rtsp::Server& rtsp_server,
        ptp::Instance& ptp_instance, Id id, uint32_t session_id, NetworkInterfaceConfig network_interface_config
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
     * @return The unique UUID of the sender.
     */
    [[nodiscard]] const boost::uuids::uuid& get_uuid() const;

    /**
     * @return The session ID of the sender.
     */
    [[nodiscard]] uint32_t get_session_id() const;

    /**
     * Updates the configuration of the sender. Only takes into account the fields in the configuration that are set.
     * This allows to update only a subset of the configuration.
     * @param config The configuration to update.
     */
    [[nodiscard]] tl::expected<void, std::string> set_configuration(Configuration config);

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
    [[nodiscard]] bool unsubscribe(const Subscriber* subscriber);

    /**
     * Sets the NMOS node for the receiver.
     * @param nmos_node The NMOS node to set. May be nullptr if NMOS is not used.
     */
    void set_nmos_node(nmos::Node* nmos_node);

    /**
     * Sets the NMOS device ID for the receiver.
     * @param device_id The device ID to set.
     */
    void set_nmos_device_id(const boost::uuids::uuid& device_id);

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
     * Sets the network interface config for the receiver.
     * @param network_interface_config The configuration of the network interface to use.
     */
    void set_network_interface_config(NetworkInterfaceConfig network_interface_config);

    /**
     * @return A JSON representation of the sender.
     */
    boost::json::object to_boost_json() const;

    /**
     * Restores the sender from a JSON representation.
     * @param json The JSON representation of the sender.
     * @return A result indicating whether the restoration was successful or not.
     */
    [[nodiscard]] tl::expected<void, std::string> restore_from_json(const boost::json::value& json);

    /**
     * @return The NMOS source of this sender.
     */
    [[nodiscard]] const nmos::SourceAudio& get_nmos_source() const;

    /**
     * @return The NMOS flow of this sender.
     */
    [[nodiscard]] const nmos::FlowAudioRaw& get_nmos_flow() const;

    /**
     * @return The NMOS sender of this sender.
     */
    [[nodiscard]] const nmos::Sender& get_nmos_sender() const;

    // rtsp_server::handler overrides
    void on_request(rtsp::Connection::RequestEvent event) const override;

    // ptp::Instance::Subscriber overrides
    void ptp_parent_changed(const ptp::ParentDs& parent) override;

  private:
    rtp::AudioSender& rtp_audio_sender_;
    dnssd::Advertiser& advertiser_;
    rtsp::Server& rtsp_server_;
    ptp::Instance& ptp_instance_;
    nmos::Node* nmos_node_ {nullptr};

    Id id_;
    uint32_t session_id_ {};
    Configuration configuration_;
    std::string rtsp_path_by_name_;
    std::string rtsp_path_by_id_;
    Id advertisement_id_;
    int32_t clock_domain_ {};
    ptp::ClockIdentity grandmaster_identity_;
    NetworkInterfaceConfig network_interface_config_;

    nmos::SourceAudio nmos_source_;
    nmos::FlowAudioRaw nmos_flow_;
    nmos::Sender nmos_sender_;

    SubscriberList<Subscriber> subscribers_;
    std::string status_message_;

    /**
     * Sends an announce request to all connected clients.
     */
    void send_announce() const;
    void update_nmos();
    void update_advertisement();
    [[nodiscard]] tl::expected<sdp::SessionDescription, std::string> build_sdp() const;
    void generate_auto_addresses_if_needed(bool notify_subscribers);
    bool generate_auto_addresses_if_needed(std::vector<Destination>& destinations) const;
    void restart_streaming() const;
    tl::expected<void, rav::nmos::ApiError> handle_patch_request(const boost::json::value& patch_request);
};

void tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const RavennaSender::Destination& destination
);

RavennaSender::Destination
tag_invoke(const boost::json::value_to_tag<RavennaSender::Destination>&, const boost::json::value& jv);

void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const RavennaSender::Configuration& config);

RavennaSender::Configuration
tag_invoke(const boost::json::value_to_tag<RavennaSender::Configuration>&, const boost::json::value& jv);

}  // namespace rav
