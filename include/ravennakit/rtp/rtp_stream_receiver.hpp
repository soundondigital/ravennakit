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
#include "ravennakit/core/util/throttle.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

/**
 * A class that receives RTP packets and buffers them for playback.
 */
class rtp_stream_receiver: public rtp_receiver::subscriber {
  public:
    class subscriber {
      public:
        virtual ~subscriber() = default;

        virtual void on_audio_format_changed(
            [[maybe_unused]] const audio_format& new_format, [[maybe_unused]] uint32_t packet_time_frames
        ) {}

        /**
         * Called when new data is available.
         *
         * The timestamp will monotonically increase, but might have gaps because out-of-order and dropped packets will
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

    explicit rtp_stream_receiver(rtp_receiver& receiver);

    ~rtp_stream_receiver() override;

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
     * Adds a subscriber to the receiver.
     * @param subscriber_to_add The subscriber to add.
     * @return true if the subscriber was added, or false if it was already added.
     */
    bool add_subscriber(subscriber* subscriber_to_add);

    /**
     * Removes a subscriber from the receiver.
     * @param subscriber_to_remove The subscriber to remove.
     * @return true if the subscriber was removed, or false if it wasn't found.
     */
    bool remove_subscriber(subscriber* subscriber_to_remove);

    /**
     * Reads data from the buffer at the given timestamp.
     * @param at_timestamp The timestamp to read from.
     * @param buffer The destination to write the data to.
     * @param buffer_size The size of the buffer in bytes.
     * @return true if buffer_size bytes were read, or false if buffer_size bytes couldn't be read.
     */
    bool read_data(uint32_t at_timestamp, uint8_t* buffer, size_t buffer_size);

    // rtp_receiver::subscriber overrides
    void on_rtp_packet(const rtp_receiver::rtp_packet_event& rtp_event) override;
    void on_rtcp_packet(const rtp_receiver::rtcp_packet_event& rtcp_event) override;

  private:
    /**
     * Context for a stream identified by the session.
     */
    struct stream_info {
        explicit stream_info(rtp_session session_) : session(std::move(session_)) {}

        rtp_session session;
        rtp_filter filter;
        wrapping_uint16 seq;
        uint16_t packet_time_frames = 0;
        std::optional<wrapping_uint32> first_packet_timestamp;
        rtp_packet_stats packet_stats;
        throttle<rtp_packet_stats::counters> packet_stats_throttle {std::chrono::seconds(2)};
    };

    static constexpr uint32_t k_delay_multiplier = 2;        // The buffer size is at least twice the delay.

    rtp_receiver& rtp_receiver_;
    audio_format selected_format_;
    std::vector<stream_info> streams_;
    uint32_t delay_ = 480;  // 100ms at 48KHz
    subscriber_list<subscriber> subscribers_;

    /// When active data is being consumed. When the FIFO is full, this will be set to false.
    std::atomic_bool consumer_active_ = true;

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
     * A struct to hold the realtime context which are variables which may be only be accesses by the consuming thread
     * (except for members which synchronize, like the fifo).
     */
    struct {
        rtp_receive_buffer receiver_buffer;
        fifo_buffer<intermediate_packet, fifo::spsc> fifo;
        fifo_buffer<uint16_t, fifo::spsc> packets_too_old;
        std::optional<wrapping_uint32> first_packet_timestamp;
        wrapping_uint32 next_ts;
        audio_format selected_audio_format;
    } realtime_context_;

    /// Restarts the stream if it is running, otherwise does nothing.
    void restart();

    stream_info& find_or_create_stream_info(const rtp_session& session);
    void handle_rtp_packet_for_stream(const rtp_packet_view& packet, stream_info& stream);
};

}  // namespace rav
