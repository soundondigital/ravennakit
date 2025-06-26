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
#include "ravennakit/core/containers/string_buffer.hpp"
#include "ravennakit/core/util/safe_function.hpp"

namespace rav::rtsp {

/**
 * Parses RTSP messages.
 */
class Parser final {
  public:
    SafeFunction<void(const Request& request)> on_request;
    SafeFunction<void(const Response& response)> on_response;

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

    Parser() = default;

    /**
     * Parses the input and returns the result.
     * When a complete message is parsed, the appropriate event is emitted.
     * @param input The input to parse.
     * @return The result of the parsing.
     */
    result parse(StringBuffer& input);

    /**
     * Resets the state to initial state. This also removes event subscribers.
     */
    void reset() noexcept;

  private:
    enum class state {
        start,
        headers,
        data,
        complete,
    } state_ {state::start};

    std::string start_line_;
    Headers headers_;
    std::string data_;

    Request request_;
    Response response_;

    result handle_response();
    result handle_request();
};

}  // namespace rav
