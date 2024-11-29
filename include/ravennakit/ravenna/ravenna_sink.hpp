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
#include "ravennakit/rtp/rtp_stream_receiver.hpp"
#include "ravennakit/rtp/detail/rtp_receiver.hpp"
#include "ravennakit/rtp/detail/rtp_filter.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

class ravenna_sink: public rtp_stream_receiver, ravenna_rtsp_client::subscriber {
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
    [[nodiscard]] std::string get_session_name() const;

  private:
    ravenna_rtsp_client& rtsp_client_;
    std::string session_name_;

    bool started_ = false;

    void on_announced(const ravenna_rtsp_client::announced_event& event) override;
};

}  // namespace rav
