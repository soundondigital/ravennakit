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
#include "rtsp_request.hpp"

namespace rav {

class rtsp_request_parser final: public rtsp_parser_base {
  public:
    using result = result;

    explicit rtsp_request_parser(rtsp_request& request) : request_(request) {}

    void reset() override {
        rtsp_parser_base::reset();
        state_ = state::method_start;
        request_.reset();
    }

  protected:
    std::string& data() override {
        return request_.data;
    }

    [[nodiscard]] std::optional<long> get_content_length() const override {
        return request_.headers.get_content_length();
    }

    result consume(char c) override;

  private:
    enum class state {
        method_start,
        method,
        uri,
        rtsp_r,
        rtsp_t,
        rtsp_s,
        rtsp_p,
        rtsp_slash,
        version_major,
        version_dot,
        version_minor,
        expecting_newline_1,
        header_start,
        header_name,
        space_before_header_value,
        header_value,
    } state_ {state::method_start};

    rtsp_request& request_;
};

}  // namespace rav
