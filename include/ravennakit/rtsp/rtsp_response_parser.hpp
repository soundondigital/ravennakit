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

#include "rtsp_parser_base.hpp"
#include "rtsp_response.hpp"

namespace rav {

/**
 * Parser for RTSP responses.
 */
class rtsp_response_parser final: public rtsp_parser_base {
  public:
    using result = result;

    explicit rtsp_response_parser(rtsp_response& response) : response_(response) {}

    void reset() override {
        rtsp_parser_base::reset();
        state_ = state::rtsp_r;
        response_.reset();
    }

  protected:
    std::string& data() override {
        return response_.data;
    }

    [[nodiscard]] std::optional<long> get_content_length() const override {
        return response_.headers.get_content_length();
    }

    result consume(char c) override;

  private:
    enum class state {
        rtsp_r,             // R
        rtsp_t,             // T
        rtsp_s,             // S
        rtsp_p,             // P
        rtsp_slash,         // /
        rtsp_1,             // 1
        rtsp_dot,           // .
        rtsp_0,             // 0
        rtsp_space,         //
        status_code_0,      // 2
        status_code_1,      // 0
        status_code_2,      // 0
        status_code_space,  //
        reason_phrase,      // OK
        header_start,
        header_name,
        space_before_header_value,
        header_value,
    } state_ {state::rtsp_r};

    rtsp_response& response_;
};

}  // namespace rav
