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

#include <exception>
#include <string>

#define RAV_THROW_EXCEPTION(msg) throw rav::exception(msg, __FILE__, __LINE__, RAV_FUNCTION)

namespace rav {

class exception: public std::exception {
  public:
    explicit
    exception(const char* msg, const char* file = nullptr, const int line = -1, const char* function_name = nullptr) :
        error_(msg), file_(file), line_(line), function_name_(function_name) {}

    explicit
    exception(std::string msg, const char* file = nullptr, const int line = -1, const char* function_name = nullptr) :
        error_(std::move(msg)), file_(file), line_(line), function_name_(function_name) {}

    /**
     * @returns The error message associated with the error code.
     */
    [[nodiscard]] const char* what() const noexcept override {
        return error_.c_str();
    }

    /**
     * @return The file where the error occurred.
     */
    [[nodiscard]] const char* file() const {
        return file_;
    }

    /**
     * @return The line number where the error occurred.
     */
    [[nodiscard]] int line() const {
        return line_;
    }

    /**
     * @return The name of the function where the error occurred.
     */
    [[nodiscard]] const char* function_name() const {
        return function_name_;
    }

  private:
    std::string error_;
    const char* file_ {};
    int line_ {};
    const char* function_name_ {};
};

}  // namespace rav
