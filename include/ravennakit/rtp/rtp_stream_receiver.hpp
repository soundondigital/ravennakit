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
#include "ravennakit/sdp/session_description.hpp"

namespace rav {

class rtp_stream_receiver: public rtp_receiver::subscriber {
  public:
    explicit rtp_stream_receiver(rtp_receiver& receiver);

    ~rtp_stream_receiver() override;

    void update_sdp(const sdp::session_description& sdp);
    void set_delay(uint32_t delay);

    void on_rtp_packet(const rtp_receiver::rtp_packet_event& rtp_event) override;
    void on_rtcp_packet(const rtp_receiver::rtcp_packet_event& rtcp_event) override;

  private:
    struct stream_info {
        rtp_session session;
        rtp_filter filter;
        uint32_t sequence_number = 0;
        uint32_t packet_time_frames = 0;
    };

    static constexpr uint32_t k_delay_multiplier = 2; // The buffer size is twice the delay.

    rtp_receiver& rtp_receiver_;
    sdp::format selected_format_;
    rtp_receive_buffer receiver_buffer_;
    std::vector<stream_info> streams_;
    uint32_t delay_ = 480;

    /// Restarts the streaming
    void restart();

    stream_info& find_or_create_stream_info(const rtp_session& session);
    void handle_rtp_packet_for_stream(const rtp_packet_view& packet, stream_info& stream);
};

}  // namespace rav
