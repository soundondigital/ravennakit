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
#include "detail/rtp_receiver.hpp"
#include "ravennakit/sdp/session_description.hpp"

namespace rav {

class rtp_stream_receiver: rtp_receiver::subscriber {
  public:
    explicit rtp_stream_receiver(rtp_receiver& receiver);

    ~rtp_stream_receiver() override;

    void update_sdp(const sdp::session_description& sdp);

  private:
    struct session_info {
        rtp_session session;
        rtp_filter filter;
    };

    rtp_receiver& rtp_receiver_;
    std::optional<session_info> primary_session_info_;
    std::optional<session_info> secondary_session_info_;
    std::optional<sdp::format> expected_format_;
    std::vector<uint8_t> buffer_;

    void reset() const;

    void on_rtp_packet(const rtp_receiver::rtp_packet_event& rtp_event) override;
    void on_rtcp_packet(const rtp_receiver::rtcp_packet_event& rtcp_event) override;
};

}  // namespace rav
