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
#include "ravennakit/sdp/session_description.hpp"

namespace rav {

class ravenna_sink {
  public:
    enum class mode { manual, automatic };

    explicit ravenna_sink(ravenna_rtsp_client& rtsp_client, std::string session_name);
    ~ravenna_sink() = default;

    void set_mode(mode new_mode);
    void set_source(std::string session_name);
    void set_manual_sdp(sdp::session_description sdp);

  private:
    ravenna_rtsp_client& rtsp_client_;
    mode mode_ = mode::automatic;
    std::string session_name_;
    std::optional<sdp::session_description> manual_sdp_;
    std::optional<sdp::session_description> auto_sdp_;
    ravenna_rtsp_client::subscriber rtsp_subscriber_;
};

}  // namespace rav
