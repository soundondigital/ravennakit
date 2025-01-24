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
#include "detail/rtp_receive_buffer.hpp"
#include "detail/rtp_receiver.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

class rtp_stream_receiver: public rtp_receiver::subscriber {
  public:
    class subscriber {
      public:
        virtual ~subscriber() = default;

        virtual void on_audio_format_changed(
            [[maybe_unused]] const audio_format& new_format, [[maybe_unused]] uint32_t packet_time_frames
        ) {}

        virtual void on_data_available([[maybe_unused]] uint32_t timestamp) {}
    };

    explicit rtp_stream_receiver(rtp_receiver& receiver);

    ~rtp_stream_receiver() override;

    void update_sdp(const sdp::session_description& sdp);
    void set_delay(uint32_t delay);
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
    bool read_data(size_t at_timestamp, uint8_t* buffer, size_t buffer_size) const;

    // rtp_receiver::subscriber overrides
    void on_rtp_packet(const rtp_receiver::rtp_packet_event& rtp_event) override;
    void on_rtcp_packet(const rtp_receiver::rtcp_packet_event& rtcp_event) override;

  private:
    struct stream_info {
        explicit stream_info(rtp_session session_) : session(std::move(session_)) {}

        rtp_session session;
        rtp_filter filter;
        uint32_t sequence_number = 0; // TODO: Make this a sequence_number type
        uint32_t packet_time_frames = 0;
        std::optional<uint32_t> first_packet_timestamp;
    };

    static constexpr uint32_t k_delay_multiplier = 2;  // The buffer size is twice the delay.

    rtp_receiver& rtp_receiver_;
    audio_format selected_format_;
    rtp_receive_buffer receiver_buffer_;
    std::vector<stream_info> streams_;
    uint32_t delay_ = 480;  // 100ms at 48KHz
    subscriber_list<subscriber> subscribers_;

    /// Restarts the streaming
    void restart();

    stream_info& find_or_create_stream_info(const rtp_session& session);
    void handle_rtp_packet_for_stream(const rtp_packet_view& packet, stream_info& stream);
};

}  // namespace rav
