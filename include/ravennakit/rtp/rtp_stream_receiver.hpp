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
#include "detail/rtp_receive_buffer.hpp"
#include "detail/rtp_receiver.hpp"
#include "ravennakit/core/exclusive_access_guard.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/core/util/throttle.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

/**
 * A class that receives RTP packets and buffers them for playback.
 */
class rtp_stream_receiver: public rtp_receiver::subscriber {
  public:
    /// The number of milliseconds after which a stream is considered inactive.
    static constexpr uint64_t k_receive_timeout_ms = 1000;

    /// The length of the receiver buffer in milliseconds.
    /// AES67 specifies at least 20 ms or 20 times the packet time, whichever is smaller, but since we're on desktop
    /// systems we go a bit higher. Note that this number is not the same as the delay or added latency.
    static constexpr size_t k_buffer_size_ms = 200;

    /**
     * The state of the stream.
     */
    enum class receiver_state {
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
    [[nodiscard]] static const char* to_string(receiver_state state);

    /**
     * Event for when this receiver changed in some way, containing the updated parameters.
     */
    struct stream_updated_event {
        id receiver_id;
        rtp_session session;
        rtp_filter filter;
        audio_format selected_audio_format;
        uint16_t packet_time_frames = 0;
        uint32_t delay_samples = 0;
        receiver_state state = receiver_state::idle;

        [[nodiscard]] std::string to_string() const;
    };

    /**
     * Baseclass for other classes which want to receive changes to the stream.
     */
    class subscriber {
      public:
        virtual ~subscriber();

        /**
         * Called when the stream has changed.
         * @param event The event.
         */
        virtual void stream_updated([[maybe_unused]] const stream_updated_event& event) {}

        /**
         * Set the rtp stream receiver, subscribing to the receiver.
         * @param receiver The receiver to set.
         */
        void set_rtp_stream_receiver(rtp_stream_receiver* receiver);

      private:
        rtp_stream_receiver* receiver_ {nullptr};
    };

    /**
     * Baseclass for other classes which want to receive data from the stream receiver.
     * Callbacks are called from the network receive thread, which might need to be synchronized.
     * Using this callback class is not mandatory as the rtp_stream_receiver allow to pull data from the buffer using
     * the `read_data` method.
     */
    class data_callback {
      public:
        virtual ~data_callback() = default;

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
        virtual void on_data_received([[maybe_unused]] wrapping_uint32 timestamp) {}

        /**
         * Called when data is ready to be consumed.
         *
         * The timestamp will be the timestamp of the packet which triggered this event, minus the delay. This makes it
         * convenient for consumers to read data from the buffer when the delay has passed.
         *
         * Note: this is called from the network receive thread. You might have to synchronize access to shared data.
         *
         * @param timestamp The timestamp of the packet which triggered this event, ** minus the delay **.
         */
        virtual void on_data_ready([[maybe_unused]] wrapping_uint32 timestamp) {}
    };

    /**
     * A struct to hold the packet and interval statistics for the stream.
     */
    struct stream_stats {
        /// The packet interval statistics.
        sliding_stats::stats packet_interval_stats;
        /// The packet statistics.
        rtp_packet_stats::counters packet_stats;
    };

    explicit rtp_stream_receiver(rtp_receiver& receiver);

    ~rtp_stream_receiver() override;

    /**
     * @returns The unique ID of this stream receiver. The id is unique across the process.
     */
    [[nodiscard]] id get_id() const;

    /**
     * Update the stream information with given SDP. Might result in a restart of the streaming if the parameters have
     * changed.
     * @param sdp The SDP to update with.
     */
    void update_sdp(const sdp::session_description& sdp);

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
     * Adds a callback to the receiver.
     * @param callback The callback to add.
     * @return true if the callback was added, or false if it was already added.
     */
    bool add_data_callback(data_callback* callback);

    /**
     * Removes a data callback from the receiver.
     * @param callback The callback to remove.
     * @return true if the callback was removed, or false if it wasn't found.
     */
    bool remove_data_callback(data_callback* callback);

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
    std::optional<uint32_t>
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
    std::optional<uint32_t>
    read_audio_data_realtime(audio_buffer_view<float> output_buffer, std::optional<uint32_t> at_timestamp);

    /**
     * @return The packet statistics for the first stream, if it exists, otherwise an empty structure.
     */
    stream_stats get_session_stats() const;

    /**
     * @return The packet statistics for the first stream, if it exists, otherwise an empty structure.
     */
    rtp_packet_stats::counters get_packet_stats() const;

    /**
     * @return The packet interval statistics for the first stream, if it exists, otherwise an empty structure.
     */
    sliding_stats::stats get_packet_interval_stats() const;

    // rtp_receiver::subscriber overrides
    void on_rtp_packet(const rtp_receiver::rtp_packet_event& rtp_event) override;
    void on_rtcp_packet(const rtp_receiver::rtcp_packet_event& rtcp_event) override;

  private:
    /**
     * Context for a stream identified by the session.
     */
    struct media_stream {
        explicit media_stream(rtp_session session_) : session(std::move(session_)) {}

        rtp_session session;
        rtp_filter filter;
        audio_format selected_format;
        uint16_t packet_time_frames = 0;
        wrapping_uint16 seq;
        std::optional<wrapping_uint32> first_packet_timestamp;
        rtp_packet_stats packet_stats;
        throttle<rtp_packet_stats::counters> packet_stats_throttle {std::chrono::seconds(5)};
        wrapping_uint64 last_packet_time_ns;
        sliding_stats packet_interval_stats {1000};
        throttle<void> packet_interval_throttle {std::chrono::seconds(10)};
    };

    rtp_receiver& rtp_receiver_;
    id id_ {id::next_process_wide_unique_id()};
    std::atomic<uint32_t> delay_ = 480;  // 100ms at 48KHz
    receiver_state state_ {receiver_state::idle};
    std::vector<media_stream> media_streams_;
    subscriber_list<subscriber> subscribers_;
    subscriber_list<data_callback> data_callbacks_;
    asio::steady_timer maintenance_timer_;
    exclusive_access_guard realtime_access_guard_;

    /**
     * Used for copying received packets to the realtime context.
     */
    struct intermediate_packet {
        uint32_t timestamp;
        uint16_t seq;
        uint16_t data_len;
        uint16_t packet_time_frames;
        std::array<uint8_t, 1500> data;  // MTU
    };

    /**
     * Bundles variables which will be accessed by a call to read_data, potentially from a realtime thread.
     */
    struct {
        rtp_receive_buffer receiver_buffer;
        std::vector<uint8_t> read_buffer;
        fifo_buffer<intermediate_packet, fifo::spsc> fifo;
        fifo_buffer<uint16_t, fifo::spsc> packets_too_old;
        std::optional<wrapping_uint32> first_packet_timestamp;
        wrapping_uint32 next_ts;
        audio_format selected_audio_format;
        /// Whether data is being consumed. When the FIFO is full, this will be set to false.
        std::atomic_bool consumer_active_ = false;
    } realtime_context_;

    /**
     * Restarts the stream if it is running, otherwise does nothing.
     */
    void restart();

    std::pair<media_stream*, bool> find_or_create_media_stream(const rtp_session& session);
    void handle_rtp_packet_event_for_session(const rtp_receiver::rtp_packet_event& event, media_stream& stream);
    void set_state(receiver_state new_state, bool notify_subscribers);
    stream_updated_event make_updated_event() const;
    void do_maintenance();
    void do_realtime_maintenance();
};

}  // namespace rav
