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

#include "detail/rtp_filter.hpp"
#include "detail/rtp_packet_stats.hpp"
#include "detail/rtp_buffer.hpp"
#include "detail/rtp_receiver.hpp"
#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/core/exclusive_access_guard.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"
#include "ravennakit/core/sync/rcu.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/core/util/throttle.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav::rtp {

/**
 * A class that receives RTP packets and buffers them for playback.
 */
class StreamReceiver: public Receiver::Subscriber {
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
     * @return A string representation of the state.
     */
    [[nodiscard]] static const char* to_string(ReceiverState state);

    /**
     * Event for when this receiver changed in some way, containing the updated parameters.
     */
    struct StreamUpdatedEvent {
        Id receiver_id;
        Session session;
        Filter filter;
        AudioFormat selected_audio_format;
        uint16_t packet_time_frames = 0;
        uint32_t delay_samples = 0;
        ReceiverState state = ReceiverState::idle;

        [[nodiscard]] std::string to_string() const;
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
         * @param event The event.
         */
        virtual void on_rtp_stream_receiver_updated([[maybe_unused]] const StreamUpdatedEvent& event) {}

        /**
         * Called when new data has been received.
         *
         * The timestamp will monotonically increase, and might have gaps because out-of-order and dropped packets will
         * not trigger a callback.
         *
         * Note: this is called from the network receive thread. You might have to synchronize access to shared data.
         *
         * @param timestamp The timestamp of newly received the data.
         */
        virtual void on_data_received([[maybe_unused]] WrappingUint32 timestamp) {}

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
        virtual void on_data_ready([[maybe_unused]] WrappingUint32 timestamp) {}
    };

    /**
     * A struct to hold the packet and interval statistics for the stream.
     */
    struct StreamStats {
        /// The packet interval statistics.
        SlidingStats::Stats packet_interval_stats;
        /// The packet statistics.
        PacketStats::Counters packet_stats;
    };

    explicit StreamReceiver(Receiver& receiver);

    ~StreamReceiver() override;

    /**
     * @returns The unique ID of this stream receiver. The id is unique across the process.
     */
    [[nodiscard]] Id get_id() const;

    /**
     * Update the stream information with given SDP. Might result in a restart of the streaming if the parameters have
     * changed.
     * @param sdp The SDP to update with.
     */
    void update_sdp(const sdp::SessionDescription& sdp);

    /**
     * Sets the delay nin samples.
     * @param delay The delay in samples.
     */
    void set_delay(uint32_t delay);

    /**
     * @return The delay in samples.
     */
    [[nodiscard]] uint32_t get_delay() const;

    /**
     * Adds a subscriber to the receiver.
     * @param subscriber_to_add The subscriber to add.
     * @return true if the subscriber was added, or false if it was already in the list.
     */
    [[nodiscard]] bool subscribe(Subscriber* subscriber_to_add);

    /**
     * Removes a subscriber from the receiver.
     * @param subscriber_to_remove The subscriber to remove.
     * @return true if the subscriber was removed, or false if it wasn't found.
     */
    [[nodiscard]] bool unsubscribe(Subscriber* subscriber_to_remove);

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
    [[nodiscard]] StreamStats get_session_stats() const;

    /**
     * @return The packet statistics for the first stream, if it exists, otherwise an empty structure.
     */
    [[nodiscard]] PacketStats::Counters get_packet_stats() const;

    /**
     * @return The packet interval statistics for the first stream, if it exists, otherwise an empty structure.
     */
    [[nodiscard]] SlidingStats::Stats get_packet_interval_stats() const;

    // rtp_receiver::subscriber overrides
    void on_rtp_packet(const Receiver::RtpPacketEvent& rtp_event) override;
    void on_rtcp_packet(const Receiver::RtcpPacketEvent& rtcp_event) override;

  private:
    /**
     * Context for a stream identified by the session.
     */
    struct MediaStream {
        explicit MediaStream(Session session_) : session(std::move(session_)) {}

        Session session;
        Filter filter;
        AudioFormat selected_format;
        uint16_t packet_time_frames = 0;
        WrappingUint16 seq;
        std::optional<WrappingUint32> rtp_ts;
        PacketStats packet_stats;
        Throttle<PacketStats::Counters> packet_stats_throttle {std::chrono::seconds(5)};
        WrappingUint64 last_packet_time_ns;
        SlidingStats packet_interval_stats {1000};
        Throttle<void> packet_interval_throttle {std::chrono::seconds(10)};
    };

    Receiver& rtp_receiver_;
    Id id_ {Id::get_next_process_wide_unique_id()};
    std::atomic<uint32_t> delay_ = 480;  // 100ms at 48KHz
    ReceiverState state_ {ReceiverState::idle};
    std::vector<MediaStream> media_streams_;
    SubscriberList<Subscriber> subscribers_;
    asio::steady_timer maintenance_timer_;
    ExclusiveAccessGuard realtime_access_guard_;

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

    struct SharedState {
        Buffer receiver_buffer;
        std::vector<uint8_t> read_buffer;
        FifoBuffer<IntermediatePacket, Fifo::Spsc> fifo;
        FifoBuffer<uint16_t, Fifo::Spsc> packets_too_old;
        std::optional<WrappingUint32> first_packet_timestamp;
        WrappingUint32 next_ts;
        AudioFormat selected_audio_format;
        /// Whether data is being consumed. When the FIFO is full, this will be set to false.
        std::atomic_bool consumer_active {false};
    };

    Rcu<SharedState> shared_state_;
    Rcu<SharedState>::Reader audio_thread_reader_ {shared_state_.create_reader()};
    Rcu<SharedState>::Reader network_thread_reader_ {shared_state_.create_reader()};

    /**
     * Restarts the stream if it is running, otherwise does nothing.
     */
    void restart();

    std::pair<MediaStream*, bool> find_or_create_media_stream(const Session& session);
    void handle_rtp_packet_event_for_session(const Receiver::RtpPacketEvent& event, MediaStream& stream);
    void set_state(ReceiverState new_state, bool notify_subscribers);
    StreamUpdatedEvent make_updated_event() const;
    void do_maintenance();
    void do_realtime_maintenance();
};

}  // namespace rav::rtp
