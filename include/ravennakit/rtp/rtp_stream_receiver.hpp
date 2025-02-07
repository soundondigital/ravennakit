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

        virtual void on_data_available([[maybe_unused]] wrapping_uint32 timestamp) {}
    };

    explicit rtp_stream_receiver(rtp_receiver& receiver);

    ~rtp_stream_receiver() override;

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

    bool add_subscriber(subscriber* subscriber_to_add);
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
    struct stream_info {
        explicit stream_info(rtp_session session_) : session(std::move(session_)) {}

        rtp_session session;
        rtp_filter filter;
        wrapping_uint16 seq;
        uint32_t packet_time_frames = 0;
        std::optional<wrapping_uint32> first_packet_timestamp;
        rtp_packet_stats packet_stats;
        throttle<rtp_packet_stats::counters> packet_stats_throttle {std::chrono::seconds(2)};
    };

    static constexpr uint32_t k_delay_multiplier = 2;        // The buffer size is at least twice the delay.
    static constexpr float k_stats_window_size_ms = 1000.f;  // 1 second time for packets to arrive

    rtp_receiver& rtp_receiver_;
    audio_format selected_format_;
    std::vector<stream_info> streams_;
    uint32_t delay_ = 480;  // 100ms at 48KHz
    subscriber_list<subscriber> subscribers_;

    /**
     * Used for copying received packets to the realtime context.
     */
    struct intermediate_packet {
        uint32_t timestamp;
        uint16_t seq;
        uint16_t len;
        std::array<uint8_t, 1500> data;  // MTU
    };

    struct {
        rtp_receive_buffer receiver_buffer;
        fifo_buffer<intermediate_packet, fifo::spsc> fifo;
        std::optional<wrapping_uint32> first_packet_timestamp;
        wrapping_uint32 next_ts;
        audio_format selected_audio_format;
    } realtime_context_;

    /// Restarts the streaming
    void restart();

    stream_info& find_or_create_stream_info(const rtp_session& session);
    void handle_rtp_packet_for_stream(const rtp_packet_view& packet, stream_info& stream);
};

}  // namespace rav
