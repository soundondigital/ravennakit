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

#include <uv.h>

#include "ravennakit/core/exception.hpp"

#define UV_THROW_EXCEPTION(uv_error_code) throw rav::uv::uv_exception(uv_error_code, __FILE__, __LINE__, RAV_FUNCTION)
#define UV_THROW_IF_ERROR(uv_error_code)   \
    if (uv_error_code < 0) {               \
        UV_THROW_EXCEPTION(uv_error_code); \
    }

namespace rav::uv {

class uv_exception final: public rav::exception {
  public:
    uv_exception() = delete;

    explicit uv_exception(
        const int uv_error_code, const char* file = nullptr, const int line = -1, const char* function_name = nullptr
    ) noexcept :
        exception({}, file, line, function_name), uv_error_code_(uv_error_code) {}

    /**
     * @returns The error message associated with the error code (as returned by uv_strerror).
     */
    [[nodiscard]] const char* what() const noexcept override {
        return uv_strerror(uv_error_code_);
    }

    /**
     * @return The error name associated with the error code (as returned by uv_err_name).
     */
    [[nodiscard]] const char* name() const {
        return uv_err_name(uv_error_code_);
    }

    /**
     * @returns The error code returned by libuv.
     */
    [[nodiscard]] int error_code() const noexcept {
        return uv_error_code_;
    }

  private:
    int uv_error_code_ {};
};

}  // namespace rav::uv
