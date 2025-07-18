/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "ravenna_rtsp_client.hpp"
#include "ravennakit/core/containers/fifo_buffer.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/nmos/nmos_node.hpp"
#include "ravennakit/nmos/models/nmos_receiver_audio.hpp"
#include "ravennakit/rtp/rtp_audio_receiver.hpp"
#include "ravennakit/rtp/detail/rtp_receiver.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

class RavennaReceiver: public RavennaRtspClient::Subscriber {
  public:
    /// List of supported audio encodings for the sender.
    static constexpr auto k_supported_encodings = {AudioEncoding::pcm_s16, AudioEncoding::pcm_s24};

    /**
     * Defines the configuration for the receiver.
     * When no sdp_text is set, but the session_name is set, the SDP will be requested from the server.
     */
    struct Configuration {
        sdp::SessionDescription sdp;
        std::string session_name;
        uint32_t delay_frames {};
        bool enabled {};
        bool auto_update_sdp {true};  // When true, the receiver will connect to the RTSP server for SDP updates.

        static Configuration default_config() {
            return Configuration {{}, {}, 480, true, true};
        }
    };

    /**
     * Baseclass for other classes which want to receive changes to the stream.
     */
    class Subscriber {
      public:
        virtual ~Subscriber() = default;

        /**
         * Called when the stream has changed.
         * @param parameters The stream parameters.
         */
        virtual void ravenna_receiver_parameters_updated(const rtp::Receiver3::ReaderParameters& parameters) {
            std::ignore = parameters;
        }

        /**
         * Called when the configuration of the stream has changed.
         * @param receiver The id of the receiver.
         * @param configuration The new configuration.
         */
        virtual void
        ravenna_receiver_configuration_updated(const RavennaReceiver& receiver, const Configuration& configuration) {
            std::ignore = receiver;
            std::ignore = configuration;
        }

        /**
         * Called when the state of a stream has changed.
         * @param stream_info The stream which changed.
         * @param state The new state of the receiver.
         */
        virtual void ravenna_receiver_stream_state_updated(
            const rtp::Receiver3::StreamInfo& stream_info, const rtp::Receiver3::StreamState state
        ) {
            std::ignore = stream_info;
            std::ignore = state;
        }

        /**
         * Called when the stats of a stream have been updated.
         * @param receiver_id The receiver for which the stats were updated.
         * @param stream_index The index of the stream which was updated.
         * @param stats The updated stats of the receiver.
         */
        virtual void
        ravenna_receiver_stream_stats_updated(
            Id receiver_id, size_t stream_index, const rtp::PacketStats::Counters& stats
        ) {
            std::ignore = receiver_id;
            std::ignore = stream_index;
            std::ignore = stats;
        }

        /**
         * Called when new data has been received.
         * The timestamp will monotonically increase, but might have gaps because out-of-order and dropped packets.
         * @param timestamp The timestamp of newly received the data.
         */
        virtual void on_data_received(const WrappingUint32 timestamp) {
            std::ignore = timestamp;
        }

        /**
         * Called when data is ready to be consumed.
         * The timestamp will be the timestamp of the packet which triggered this event, minus the delay. This makes it
         * convenient for consumers to read data from the buffer when the delay has passed. There will be no gaps in
         * timestamp as newer packets will trigger this event for lost packets, and out of order packet (which are
         * basically lost, not lost but late packets) will be ignored.
         * @param timestamp The timestamp of the packet which triggered this event, ** minus the delay **.
         */
        virtual void on_data_ready(const WrappingUint32 timestamp) {
            std::ignore = timestamp;
        }
    };

    explicit RavennaReceiver(RavennaRtspClient& rtsp_client, rtp::Receiver3& receiver3, Id id);
    ~RavennaReceiver() override;

    RavennaReceiver(const RavennaReceiver&) = delete;
    RavennaReceiver& operator=(const RavennaReceiver&) = delete;

    RavennaReceiver(RavennaReceiver&&) noexcept = delete;
    RavennaReceiver& operator=(RavennaReceiver&&) noexcept = delete;

    /**
     * @returns The unique ID of this stream receiver. The id is unique across the process.
     */
    [[nodiscard]] Id get_id() const;

    /**
     * @return The unique UUID of the receiver.
     */
    [[nodiscard]] const boost::uuids::uuid& get_uuid() const;

    /**
     * Updates the configuration of the receiver. Only takes into account the fields in the configuration that are set.
     * This allows to update only a subset of the configuration.
     * @param config The configuration changes to apply.
     */
    [[nodiscard]] tl::expected<void, std::string> set_configuration(Configuration config);

    /**
     * @returns The current configuration of the receiver.
     */
    [[nodiscard]] const Configuration& get_configuration() const;

    /**
     * Adds a subscriber to the receiver.
     * @param subscriber The subscriber to add.
     * @return true if the subscriber was added, or false if it was already in the list.
     */
    [[nodiscard]] bool subscribe(Subscriber* subscriber);

    /**
     * Removes a subscriber from the receiver.
     * @param subscriber The subscriber to remove.
     * @return true if the subscriber was removed, or false if it wasn't found.
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
     * @return The SDP for the session.
     */
    [[nodiscard]] std::optional<sdp::SessionDescription> get_sdp() const;

    /**
     * @return The SDP text for the session. This is the original SDP text as receiver from the server potentially
     * including things which haven't been parsed into the session_description.
     */
    [[nodiscard]] std::optional<std::string> get_sdp_text() const;

    /**
     * Sets the network interface config for the receiver.
     * @param network_interface_config The configuration of the network interface to use.
     */
    void set_network_interface_config(NetworkInterfaceConfig network_interface_config);

    /**
     * @return A JSON representation of the sender.
     */
    [[nodiscard]] boost::json::object to_boost_json() const;

    /**
     * Restores the receiver from a JSON representation.
     * @param json The JSON representation of the receiver.
     * @return A result indicating whether the restoration was successful or not.
     */
    [[nodiscard]] tl::expected<void, std::string> restore_from_json(const boost::json::value& json);

    /**
     * Reads data from the buffer at the given timestamp.
     * TODO: Remove this function and call into rtp::Receiver3 directly
     *
     * Calling this function is realtime safe and thread safe when called from a single arbitrary thread.
     *
     * @param buffer The destination to write the data to.
     * @param buffer_size The size of the buffer in bytes.
     * @param at_timestamp The optional timestamp to read at. If nullopt, the most recent timestamp minus the delay will
     * be used for the first read and after that the timestamp will be incremented by the packet time.
     * @return The timestamp at which the data was read, or std::nullopt if an error occurred.
     */
    [[nodiscard]] std::optional<uint32_t>
    read_data_realtime(uint8_t* buffer, size_t buffer_size, std::optional<uint32_t> at_timestamp);

    /**
     * Reads the data from the receiver with the given id.
     * TODO: Remove this function and call into rtp::Receiver3 directly
     *
     * Calling this function is realtime safe and thread safe when called from a single arbitrary thread.
     *
     * @param output_buffer The buffer to read the data into.
     * @param at_timestamp The optional timestamp to read at. If nullopt, the most recent timestamp minus the delay will
     * be used for the first read and after that the timestamp will be incremented by the packet time.
     * @return The timestamp at which the data was read, or std::nullopt if an error occurred.
     */
    [[nodiscard]] std::optional<uint32_t>
    read_audio_data_realtime(const AudioBufferView<float>& output_buffer, std::optional<uint32_t> at_timestamp);

    /**
     * @return The NMOS receiver of this receiver.
     */
    [[nodiscard]] const nmos::ReceiverAudio& get_nmos_receiver() const;

    /**
     * Call regularly from the maintenance thread.
     */
    void do_maintenance();

    // ravenna_rtsp_client::subscriber overrides
    void on_announced(const RavennaRtspClient::AnnouncedEvent& event) override;

  private:
    RavennaRtspClient& rtsp_client_;
    rtp::Receiver3& rtp_receiver_;
    nmos::Node* nmos_node_ {nullptr};
    const Id id_;
    Configuration configuration_;
    NetworkInterfaceConfig network_interface_config_;
    nmos::ReceiverAudio nmos_receiver_;
    SubscriberList<Subscriber> subscribers_;
    rtp::Receiver3::ReaderParameters reader_parameters_;
    std::array<rtp::Receiver3::StreamState, rtp::Receiver3::k_max_num_redundant_sessions> streams_states_;
    Throttle<void> stats_throttle_ {std::chrono::seconds(1)};

    void handle_announced_sdp(const sdp::SessionDescription& sdp);
    tl::expected<void, std::string> update_nmos();
    tl::expected<void, std::string> update_rtsp();
};

/**
 * Creates rtp receiver parameters from the given SDP.
 * @param sdp The SDP to create the parameters from.
 * @return The rtp receiver parameters, or an error message if the SDP is invalid.
 */
tl::expected<rtp::Receiver3::ReaderParameters, std::string>
create_rtp_receiver_parameters(const sdp::SessionDescription& sdp);

void tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const RavennaReceiver::Configuration& config
);

RavennaReceiver::Configuration
tag_invoke(const boost::json::value_to_tag<RavennaReceiver::Configuration>&, const boost::json::value& jv);

}  // namespace rav
