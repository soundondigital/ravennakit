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
#include "ravennakit/core/util/exclusive_access_guard.hpp"
#include "ravennakit/core/containers/fifo_buffer.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/rtp/rtp_audio_receiver.hpp"
#include "ravennakit/rtp/detail/rtp_filter.hpp"
#include "ravennakit/rtp/detail/rtp_packet_stats.hpp"
#include "ravennakit/rtp/detail/rtp_receiver.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

#include <boost/uuid/random_generator.hpp>

namespace rav {

class RavennaReceiver: public RavennaRtspClient::Subscriber {
  public:
    /**
     * Defines the configuration for the receiver.
     */
    struct Configuration {
        std::string session_name;
        uint32_t delay_frames {};
        bool enabled {};

        /**
         * @return The configuration as a JSON object.
         */
        [[nodiscard]] nlohmann::json to_json() const;
    };

    /**
     * Struct for updating the configuration of the receiver. Only the fields that are set are taken into account, which
     * allows for partial updates.
     */
    struct ConfigurationUpdate {
        std::optional<std::string> session_name;
        std::optional<uint32_t> delay_frames;
        std::optional<bool> enabled;

        /**
         * Creates a configuration update from a JSON object.
         * @param json The JSON object to convert.
         * @return A configuration update object if the JSON is valid, otherwise an error message.
         */
        static tl::expected<ConfigurationUpdate, std::string> from_json(const nlohmann::json& json);
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
        virtual void ravenna_receiver_parameters_updated(const rtp::AudioReceiver::Parameters& parameters) {
            std::ignore = parameters;
        }

        /**
         * Called when the configuration of the stream has changed.
         * @param receiver_id The id of the receiver.
         * @param configuration The new configuration.
         */
        virtual void ravenna_receiver_configuration_updated(const Id receiver_id, const Configuration& configuration) {
            std::ignore = receiver_id;
            std::ignore = configuration;
        }

        /**
         * Called when the state of a stream has changed.
         * @param stream The stream which changed.
         * @param state The new state of the receiver.
         */
        virtual void ravenna_receiver_stream_state_updated(
            const rtp::AudioReceiver::Stream& stream, const rtp::AudioReceiver::State state
        ) {
            std::ignore = stream;
            std::ignore = state;
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

    explicit RavennaReceiver(
        boost::asio::io_context& io_context, RavennaRtspClient& rtsp_client, rtp::Receiver& rtp_receiver, Id id,
        ConfigurationUpdate initial_config = {}
    );
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
     * @param update The configuration changes to apply.
     */
    [[nodiscard]] tl::expected<void, std::string> set_configuration(const ConfigurationUpdate& update);

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
     * @return The SDP for the session.
     */
    std::optional<sdp::SessionDescription> get_sdp() const;

    /**
     * @return The SDP text for the session. This is the original SDP text as receiver from the server potentially
     * including things which haven't been parsed into the session_description.
     */
    [[nodiscard]] std::optional<std::string> get_sdp_text() const;

    /**
     * Sets the interface address for the receiver.
     * @param interface_addresses A map of interface addresses to set. The key is the rank of the interface address.
     */
    void set_interfaces(const std::map<Rank, boost::asio::ip::address_v4>& interface_addresses);

    /**
     * @return A JSON representation of the sender.
     */
    [[nodiscard]] nlohmann::json to_json() const;

    /**
     * Restores the receiver from a JSON representation.
     * @param json The JSON representation of the receiver.
     * @return A result indicating whether the restoration was successful or not.
     */
    [[nodiscard]] tl::expected<void, std::string> restore_from_json(const nlohmann::json& json);

    /**
     * Reads data from the buffer at the given timestamp.
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
     * @return The packet statistics for the first stream, if it exists, otherwise an empty structure.
     */
    [[nodiscard]] rtp::AudioReceiver::SessionStats get_stream_stats(Rank rank) const;

    // ravenna_rtsp_client::subscriber overrides
    void on_announced(const RavennaRtspClient::AnnouncedEvent& event) override;

    /**
     * Creates audio receiver parameters from the given SDP.
     * @param sdp The SDP to create the parameters from.
     * @return The audio receiver parameters, or an error message if the SDP is invalid.
     */
    static tl::expected<rtp::AudioReceiver::Parameters, std::string>
    create_audio_receiver_parameters(const sdp::SessionDescription& sdp);

  private:
    RavennaRtspClient& rtsp_client_;
    boost::uuids::uuid uuid_ = boost::uuids::random_generator()();
    rtp::AudioReceiver rtp_audio_receiver_;
    Id id_;
    Configuration configuration_;
    SubscriberList<Subscriber> subscribers_;

    void update_sdp(const sdp::SessionDescription& sdp);
};

}  // namespace rav
