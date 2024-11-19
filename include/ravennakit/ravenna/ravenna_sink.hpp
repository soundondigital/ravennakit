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
#include "ravennakit/rtp/rtp_receiver.hpp"
#include "ravennakit/rtp/detail/rtp_filter.hpp"
#include "ravennakit/sdp/session_description.hpp"

namespace rav {

class ravenna_sink: ravenna_rtsp_client::subscriber, rtp_receiver::subscriber {
  public:
    explicit ravenna_sink(ravenna_rtsp_client& rtsp_client, rtp_receiver& rtp_receiver, std::string session_name);
    ~ravenna_sink() override;

    ravenna_sink(const ravenna_sink&) = delete;
    ravenna_sink& operator=(const ravenna_sink&) = delete;

    ravenna_sink(ravenna_sink&&) noexcept = delete;
    ravenna_sink& operator=(ravenna_sink&&) noexcept = delete;

    void start();
    void stop();
    void set_session_name(std::string session_name);

  private:
    ravenna_rtsp_client& rtsp_client_;
    rtp_receiver& rtp_receiver_;
    std::string session_name_;

    struct settings {
        rtp_session session;
        sdp::format format;
        rtp_filter filter;
    };

    std::optional<settings> current_settings_{};

    bool started_ = false;

    void on(const rtp_receiver::rtp_packet_event& rtp_event) override;
    void on(const rtp_receiver::rtcp_packet_event& rtcp_event) override;
    void on(const ravenna_rtsp_client::announced_event& event) override;
};

}  // namespace rav
