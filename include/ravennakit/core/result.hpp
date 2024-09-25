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

#include <optional>
#include <variant>

namespace rav {

/**
 * A result type which holds either a value or an error.
 * @tparam T The value type.
 * @tparam E The error type.
 */
template<class T, class E>
class result {
  public:
    /**
     * Creates an ok result with given value.
     * @param value The value.
     * @return The result.
     */
    static result ok(T value) {
        return result(std::variant<T, E> {std::in_place_index<0>, std::move(value)});
    }

    /**
     * Creates an error result with given error.
     * @param error The error.
     * @return The result.
     */
    static result err(E error) {
        return result(std::variant<T, E> {std::in_place_index<1>, std::move(error)});
    }

    /**
     * @returns True if the result is ok, false otherwise.
     */
    [[nodiscard]] bool is_ok() const {
        return value_.index() == 0;
    }

    /**
     * @return True if the result is an error, false otherwise.
     */
    [[nodiscard]] bool is_err() const {
        return !is_ok();
    }

    /**
     * @return The value if the result is ok. Otherwise, throws an exception.
     */
    [[nodiscard]] const T& get_ok() const {
        return std::get<0>(value_);
    }

    /**
     * Moves the value out of this result.
     * @returns The value if the result is ok. Otherwise, throws an exception.
     */
    [[nodiscard]] T move_ok() {
        return std::move(std::get<0>(value_));
    }

    /**
     * @return The error if the result is an error. Otherwise, throws an exception.
     */
    [[nodiscard]] const E& get_err() const {
        return std::get<1>(value_);
    }

  private:
    std::variant<T, E> value_;

    explicit result(std::variant<T, E> value) : value_(std::move(value)) {}
};

/**
 * A result type specialization for void.
 * @tparam E The error type.
 */
template<class E>
class result<void, E> {
  public:
    /**
     * Creates an ok result with given value.
     * @return The result.
     */
    static result ok() {
        return result();
    }

    /**
     * Creates an error result with given error.
     * @param error The error.
     * @return The result.
     */
    static result err(E error) {
        return result(std::move(error));
    }

    /**
     * @returns True if the result is ok, false otherwise.
     */
    [[nodiscard]] bool is_ok() const {
        return !error_.has_value();
    }

    /**
     * @returns True if the result is an error, false otherwise.
     */
    [[nodiscard]] bool is_err() const {
        return !is_ok();
    }

    /**
     * @returns The error if the result is an error. Otherwise, throws an exception.
     */
    [[nodiscard]] const E& get_err() const {
        return error_.value();
    }

  private:
    std::optional<E> error_;

    result() = default;

    explicit result(E error) : error_(std::move(error)) {}
};

}  // namespace rav
