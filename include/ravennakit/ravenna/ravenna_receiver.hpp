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

class ravenna_receiver: public rtp_stream_receiver, ravenna_rtsp_client::subscriber {
  public:
    explicit ravenna_receiver(ravenna_rtsp_client& rtsp_client, rtp_receiver& rtp_receiver);
    ~ravenna_receiver() override;

    ravenna_receiver(const ravenna_receiver&) = delete;
    ravenna_receiver& operator=(const ravenna_receiver&) = delete;

    ravenna_receiver(ravenna_receiver&&) noexcept = delete;
    ravenna_receiver& operator=(ravenna_receiver&&) noexcept = delete;

    /**
     * Stops the receiver.
     */
    void stop();

    /**
     * Sets the name of the RAVENNA session to receive.
     * @param session_name
     */
    void set_session_name(std::string session_name);

    /**
     * @return The name of the RAVENNA session to receive.
     */
    [[nodiscard]] std::string get_session_name() const;

  private:
    ravenna_rtsp_client& rtsp_client_;
    std::string session_name_;

    void on_announced(const ravenna_rtsp_client::announced_event& event) override;
};

}  // namespace rav
