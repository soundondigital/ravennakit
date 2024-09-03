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

#include <uvw.hpp>
#include <variant>

#include "errors.hpp"
#include "log.hpp"

#ifdef RAV_ENABLE_SPDLOG
    #define RESULT(guts) ::rav::result(guts, __FILE__, __LINE__, RAV_FUNCTION)
#else
    #define RESULT(guts) ::rav::result(guts)
#endif

namespace rav {

/**
 * This class holds the result of an operation that may fail. It can hold an error specific to this library, or an error
 * code as returned by libuv (uvw).
 */
class result {
  public:
    result() = default;

    template<class T>
    result& operator=(const T& error) {
        error_.emplace<T>(error);
        return *this;
    }

    template<class T>
    result& operator=(T&& error) {
        error_.emplace<T>(std::forward<T>(error));
        return *this;
    }

    /**
     * Constructs a result object from a library specific error code.
     * @param error The library specific error code.
     */
    template<class T>
    explicit result(const T error) : error_(error) {}

#ifdef RAV_ENABLE_SPDLOG
    /**
     * Constructs a result object from a library specific error code.
     * @param error The library specific error code.
     * @param file The file where the error occurred.
     * @param line_num The line number where the error occurred.
     * @param function_name The name of the function where the error occurred.
     */
    template<class T>
    explicit result(const T error, const char* file, const int line_num, const char* function_name) :
        error_(error), file_(file), line_(line_num), function_name_(function_name) {}
#endif

    /**
     * @returns True if the result object represents an error, false otherwise.
     */
    [[nodiscard]] bool holds_error() const {
        return !std::holds_alternative<no_error>(error_);
    }

    /**
     * @returns True if the result object represents a success, false otherwise.
     */
    [[nodiscard]] bool is_ok() const {
        return !holds_error();
    }

    /**
     * @returns The error message associated with the error code.
     */
    [[nodiscard]] const char* what() const;

    /**
     * @return The error name associated with the error code.
     */
    [[nodiscard]] const char* name() const;

    /**
     * Tests whether this result holds an error of type T and if so, whether the error matches the given error.
     * @tparam T The error type.
     * @param error The error to test.
     * @returns True if this result object holds the given error, false otherwise.
     */
    template<class T>
    bool holds_error_of_type(const T& error) const {
        if (auto* e = std::get_if<T>(&error_)) {
            return *e == error;
        }
        return false;
    }

    /**
     * Logs an error message if this result object holds an error.
     */
    void log_if_error() const {
        if (holds_error()) {
#ifdef RAV_ENABLE_SPDLOG
            const spdlog::source_loc loc {file_, line_, function_name_};
            spdlog::default_logger_raw()->log(loc, spdlog::level::err, "Error: {}", what());
#else
            RAV_ERROR("Error: {}", what());
#endif
        }
    }

  private:
    struct no_error {};

    std::variant<no_error, error, uvw::error_event> error_ {};
#ifdef RAV_ENABLE_SPDLOG
    const char* file_ {};
    int line_ {};
    const char* function_name_ {};
#endif
};

static_assert(std::is_copy_constructible_v<uvw::error_event>);
static_assert(std::is_copy_assignable_v<uvw::error_event>);

inline result ok() {
    return {};
}

}  // namespace rav
