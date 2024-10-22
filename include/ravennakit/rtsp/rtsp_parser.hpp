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

#include "rtsp_request.hpp"
#include "rtsp_response.hpp"
#include "ravennakit/containers/string_buffer.hpp"
#include "ravennakit/core/event_emitter.hpp"

namespace rav {

/**
 * Parses RTSP messages.
 */
class rtsp_parser final: public event_emitter<rtsp_parser, rtsp_request, rtsp_response> {
  public:
    /**
     * The status of parsing.
     */
    enum class result {
        good,
        indeterminate,
        bad_method,
        bad_uri,
        bad_protocol,
        bad_version,
        bad_header,
        bad_end_of_headers,
        bad_status_code,
        bad_reason_phrase,
        unexpected_blank_line,
    };

    rtsp_parser() = default;

    /**
     * Parses the input and returns the result.
     * When a complete message is parsed, the appropriate event is emitted.
     * @param input The input to parse.
     * @return The result of the parsing.
     */
    result parse(string_buffer& input);

    /**
     * Resets the state to initial state. This also removes event subscribers.
     */
    void reset() noexcept override;

  private:
    enum class state {
        start,
        headers,
        data,
        complete,
    } state_ {state::start};

    std::string start_line_;
    rtsp_headers headers_;
    std::string data_;

    rtsp_request request_;
    rtsp_response response_;

    result handle_response();
    result handle_request();
};

}  // namespace rav
