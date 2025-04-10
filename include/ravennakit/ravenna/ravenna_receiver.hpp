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
#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/core/exclusive_access_guard.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/containers/fifo_buffer.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"
#include "ravennakit/core/sync/rcu.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/core/util/throttle.hpp"
#include "ravennakit/rtp/detail/rtp_buffer.hpp"
#include "ravennakit/rtp/detail/rtp_packet_stats.hpp"
#include "ravennakit/rtp/detail/rtp_receiver.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

class RavennaReceiver: public RavennaRtspClient::Subscriber {
  public:
    /// The number of milliseconds after which a stream is considered inactive.
    static constexpr uint64_t k_receive_timeout_ms = 1000;

    /// The length of the receiver buffer in milliseconds.
    /// AES67 specifies at least 20 ms or 20 times the packet time, whichever is smaller, but since we're on desktop
    /// systems we go a bit higher. Note that this number is not the same as the delay or added latency.
    static constexpr uint32_t k_buffer_size_ms = 200;

    /**
     * The state of the stream.
     */
    enum class ReceiverState {
        /// The stream is idle and is expected to because no SDP has been set.
        idle,
        /// An SDP has been set and the stream is waiting for the first data.
        waiting_for_data,
        /// The stream is running, packets are being received and consumed.
        ok,
        /// The stream is running, packets are being received but not consumed.
        ok_no_consumer,
        /// The stream is inactive because no packets are being received.
        inactive,
    };

    /**
     * Struct to hold the parameters of the stream.
     */
    struct StreamParameters {
        rtp::Session session;
        rtp::Filter filter;
        AudioFormat audio_format;
        uint16_t packet_time_frames = 0;
        ReceiverState state {ReceiverState::idle};

        [[nodiscard]] std::string to_string() const;
    };

    /**
     * A struct to hold the packet and interval statistics for the stream.
     */
    struct StreamStats {
        /// The packet interval statistics.
        SlidingStats::Stats packet_interval_stats;
        /// The packet statistics.
        rtp::PacketStats::Counters packet_stats;
    };

    /**
     * Defines the configuration for the receiver.
     */
    struct Configuration {
        std::string session_name;
        uint32_t delay_frames {};
        bool enabled {};
    };

    /**
     * Field to update in the configuration. Only the fields that are set are taken into account, which allows for
     * partial updates.
     */
    struct ConfigurationUpdate {
        std::optional<std::string> session_name;
        std::optional<uint32_t> delay_frames;
        std::optional<bool> enabled;
    };

    /**
     * Baseclass for other classes which want to receive changes to the stream.
     */
    class Subscriber {
      public:
        virtual ~Subscriber() = default;

        /**
         * Called when the stream has changed.
         *
         * Note: this will be called from the maintenance thread, so you might have to synchronize access to shared
         * data.
         *
         * @param parameters The stream parameters.
         */
        virtual void ravenna_receiver_stream_updated(const StreamParameters& parameters) {
            std::ignore = parameters;
        }

        /**
         * Called when the configuration of the stream has changed.
         * @param receiver_id The id of the receiver.
         * @param configuration The new configuration.
         */
        virtual void ravenna_receiver_configuration_updated(Id receiver_id, const Configuration& configuration) {
            std::ignore = receiver_id;
            std::ignore = configuration;
        }

        /**
         * Called when new data has been received.
         *
         * The timestamp will monotonically increase, but might have gaps because out-of-order and dropped packets.
         *
         * Note: this is called from the network receive thread. You might have to synchronize access to shared data.
         *
         * @param timestamp The timestamp of newly received the data.
         */
        virtual void on_data_received(WrappingUint32 timestamp) {
            std::ignore = timestamp;
        }

        /**
         * Called when data is ready to be consumed.
         *
         * The timestamp will be the timestamp of the packet which triggered this event, minus the delay. This makes it
         * convenient for consumers to read data from the buffer when the delay has passed. There will be no gaps in
         * timestamp as newer packets will trigger this event for lost packets, and out of order packet (which are
         * basically lost, not lost but late packets) will be ignored.
         *
         * Note: this is called from the network receive thread. You might have to synchronize access to shared data.
         *
         * @param timestamp The timestamp of the packet which triggered this event, ** minus the delay **.
         */
        virtual void on_data_ready(WrappingUint32 timestamp) {
            std::ignore = timestamp;
        }
    };

    explicit RavennaReceiver(
        RavennaRtspClient& rtsp_client, rtp::Receiver& rtp_receiver, ConfigurationUpdate initial_config = {}
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
     * Updates the configuration of the receiver. Only takes into account the fields in the configuration that are set.
     * This allows to update only a subset of the configuration.
     * @param update The configuration to update.
     */
    [[nodiscard]] tl::expected<void, std::string> update_configuration(const ConfigurationUpdate& update);

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
    [[nodiscard]] bool unsubscribe(Subscriber* subscriber);

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
    read_audio_data_realtime(AudioBufferView<float> output_buffer, std::optional<uint32_t> at_timestamp);

    /**
     * @return The packet statistics for the first stream, if it exists, otherwise an empty structure.
     */
    [[nodiscard]] StreamStats get_stream_stats() const;

    /**
     * @return The packet statistics for the first stream, if it exists, otherwise an empty structure.
     */
    [[nodiscard]] rtp::PacketStats::Counters get_packet_stats() const;

    /**
     * @return The packet interval statistics for the first stream, if it exists, otherwise an empty structure.
     */
    [[nodiscard]] SlidingStats::Stats get_packet_interval_stats() const;

    /**
     * @return A string representation of ReceiverState.
     */
    [[nodiscard]] static const char* to_string(ReceiverState state);

    // ravenna_rtsp_client::subscriber overrides
    void on_announced(const RavennaRtspClient::AnnouncedEvent& event) override;

  private:
    /**
     * Handless a single RTP stream
     * Note: I think this can be a class in itself, something called rtp::StreamReceiver or rtp::SessionReceiver.
     * This class would be responsible for receiving the RTP packets for a single session and provide access to the fifo
     * for lock free and thread safe access of the packets.
     * Then a class like RavennaReceiver would be reading the packets from the fifo and putting them into a buffer. The
     * good thing here is that packets from multiple sessions can be placed into the same buffer which basically gives
     * the redundancy we need at some point.
     */
    class MediaStream: public rtp::Receiver::Subscriber {
      public:
        explicit MediaStream(RavennaReceiver& owner, rtp::Receiver& rtp_receiver, rtp::Session session);
        ~MediaStream() override;
        bool update_parameters(const StreamParameters& new_parameters);
        [[nodiscard]] const rtp::Session& get_session() const;
        void do_maintenance();
        [[nodiscard]] StreamStats get_stream_stats() const;
        [[nodiscard]] rtp::PacketStats::Counters get_packet_stats() const;
        [[nodiscard]] SlidingStats::Stats get_packet_interval_stats() const;
        [[nodiscard]] const StreamParameters& get_parameters() const;

        void start();
        void stop();

        // rtp::Receiver::Subscriber overrides
        void on_rtp_packet(const rtp::Receiver::RtpPacketEvent& rtp_event) override;
        void on_rtcp_packet(const rtp::Receiver::RtcpPacketEvent& rtcp_event) override;

      private:
        RavennaReceiver& owner_;
        rtp::Receiver& rtp_receiver_;
        StreamParameters parameters_;
        WrappingUint16 seq_;
        std::optional<WrappingUint32> rtp_ts_;
        rtp::PacketStats packet_stats_;
        Throttle<rtp::PacketStats::Counters> packet_stats_throttle_ {std::chrono::seconds(5)};
        WrappingUint64 last_packet_time_ns_;
        SlidingStats packet_interval_stats_ {1000};
        Throttle<void> packet_interval_throttle_ {std::chrono::seconds(10)};
        bool is_running_ {false};

        void set_state(ReceiverState state);
    };

    /**
     * Used for copying received packets to the realtime context.
     */
    struct IntermediatePacket {
        uint32_t timestamp;
        uint16_t seq;
        uint16_t data_len;
        uint16_t packet_time_frames;
        std::array<uint8_t, aes67::constants::k_max_payload> data;
    };

    struct SharedContext {
        rtp::Buffer receiver_buffer;
        std::vector<uint8_t> read_buffer;
        FifoBuffer<IntermediatePacket, Fifo::Spsc> fifo;
        FifoBuffer<uint16_t, Fifo::Spsc> packets_too_old;
        std::optional<WrappingUint32> first_packet_timestamp;
        WrappingUint32 next_ts;
        AudioFormat selected_audio_format;
        uint32_t delay_frames = 0;
        /// Whether data is being consumed. When the FIFO is full, this will be set to false.
        std::atomic_bool consumer_active {false};
    };

    rtp::Receiver& rtp_receiver_;
    RavennaRtspClient& rtsp_client_;
    Configuration configuration_;
    SubscriberList<Subscriber> subscribers_;

    Id id_ {Id::get_next_process_wide_unique_id()};
    std::vector<std::unique_ptr<MediaStream>> media_streams_;
    asio::steady_timer maintenance_timer_;
    ExclusiveAccessGuard realtime_access_guard_;

    Rcu<SharedContext> shared_context_;
    Rcu<SharedContext>::Reader audio_thread_reader_ {shared_context_.create_reader()};
    Rcu<SharedContext>::Reader network_thread_reader_ {shared_context_.create_reader()};

    void update_sdp(const sdp::SessionDescription& sdp);
    void update_shared_context();

    /// @returns The stream with the given session, or a new stream if it does not exist. The second value indicates
    /// whether the stream was created.
    std::pair<MediaStream*, bool> find_or_create_media_stream(const rtp::Session& session);
    void do_maintenance();
    void do_realtime_maintenance();
};

}  // namespace rav
