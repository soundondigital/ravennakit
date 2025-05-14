/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/constants.hpp"
#include "ravennakit/core/string_parser.hpp"

#include <map>
#include <string_view>
#include <boost/system/result.hpp>
#include <fmt/ostream.h>

namespace rav {

/**
 * A simple path matcher that matches paths against patterns.
 * For example, it can be used to match URLs against patterns like "/user/{id}" or "/user/ **".
 *
 * Path parameters are enclosed in curly braces, e.g. "/user/{id}" and fed into an instance of Parameters.
 * The single wildcard "*" matches any single path segment. It must appear without additional characters.
 * The double wildcard "**" matches any path below the current level. It can only appear at the end of the pattern.
 *
 * Look at the unit tests for examples of usage.
 */
class PathMatcher {
  public:
    enum class Error {
        invalid_recursive_wildcard,
        invalid_argument,
    };

    /**
     * Contains parameters extracted from a path with convenience functions to convert to integers.
     */
    class Parameters {
      public:
        /**
         * Set a parameter with the given name and value.
         * @param name The parameter name (e.g. "id" in "{id}").
         * @param value The parameter value.
         */
        void set(const std::string_view name, const std::string_view value) {
            parameters_[std::string(name)] = std::string(value);
        }

        /**
         * Get a parameter value by name.
         * @param name The parameter name (e.g. "id" in "{id}").
         * @return The parameter value, or nullptr if the parameter was not found.
         */
        [[nodiscard]] const std::string* get(const std::string_view name) const {
            const auto it = parameters_.find(std::string(name));
            if (it != parameters_.end()) {
                return &it->second;
            }
            return nullptr;
        }

        /**
         * Get a parameter value by name and convert it to an integer of type T.
         * @tparam T The type to convert to. Must be an integral type (e.g. int, long, etc.).
         * @param name The parameter name (e.g. "id" in "{id}").
         * @return The parameter value as an optional of type T, or an empty optional if the parameter was not found or
         * the conversion failed.
         */
        template<typename T>
        std::enable_if_t<std::is_integral_v<T>, std::optional<T>> get_as(const std::string_view name) const {
            const auto it = parameters_.find(std::string(name));
            if (it != parameters_.end()) {
                return string_to_int<T>(it->second);
            }
            return std::nullopt;
        }

        /**
         * Get all parameters as a map.
         * @return A map of parameter names to values.
         */
        [[nodiscard]] const std::map<std::string, std::string>& get_all() const {
            return parameters_;
        }

        /**
         * @return True if no parameters were set, false otherwise.
         */
        [[nodiscard]] bool empty() const {
            return parameters_.empty();
        }

        /**
         * Clear all parameters.
         */
        void clear() {
            parameters_.clear();
        }

      private:
        std::map<std::string, std::string> parameters_;  // name => value
    };

    /**
     * Match a path against a pattern.
     * Note: if the pattern contains a parameter, the parameter argument must not be null, otherwise the function will
     * return false.
     * @param path The path to match (e.g. "/user/123").
     * @param pattern The pattern to match against (e.g. "/user/{id}").
     * @param parameters The parameters to fill with the extracted values from the path.
     * @return True if the path matches the pattern, false otherwise, or an error if the pattern or arguments are
     * invalid.
     */
    [[nodiscard]] static boost::system::result<bool, Error>
    match(const std::string_view path, const std::string_view pattern, Parameters* parameters = nullptr) {
        if (path.empty() || pattern.empty()) {
            return false;
        }

        StringParser path_parser(path);
        StringParser pattern_parser(pattern);

        path_parser.skip('/');
        pattern_parser.skip('/');

        auto path_section = path_parser.split('/');
        auto pattern_section = pattern_parser.split('/');

        for (size_t i = 0; i < RAV_LOOP_UPPER_BOUND; ++i) {
            if (!path_section && !pattern_section) {
                return true;
            }

            if (pattern_section == "**") {
                if (!pattern_parser.exhausted()) {
                    return Error::invalid_recursive_wildcard;
                }
                return true;
            }

            if (!path_section || !pattern_section) {
                return false;
            }

            if (path_section == pattern_section || pattern_section == "*") {
                path_section = path_parser.split('/');
                pattern_section = pattern_parser.split('/');
                continue;
            }

            StringParser parameter_parser(*pattern_section);
            const auto leading = parameter_parser.read_until('{');
            const auto parameter_name = parameter_parser.read_until('}');
            const auto trailing = parameter_parser.read_until_end();

            if (!leading) {
                return false;  // No '{' was found
            }

            if (!parameter_name || parameter_name->empty()) {
                return false;
            }

            auto parameter_value = *path_section;

            if (leading) {
                bool found = false;
                parameter_value = string_remove_prefix(parameter_value, *leading, &found);
                if (!found) {
                    return false;
                }
            }

            if (trailing) {
                bool found = false;
                parameter_value = string_remove_suffix(parameter_value, *trailing, &found);
                if (!found) {
                    return false;
                }
            }

            if (parameters == nullptr) {
                return Error::invalid_argument;
            }

            parameters->set(*parameter_name, parameter_value);

            path_section = path_parser.split('/');
            pattern_section = pattern_parser.split('/');
        }

        RAV_ASSERT_FALSE("PathMatcher::match() loop upper bound exceeded");
        return false;
    }
};

// Make PathMatcher::Error compatible with ostream
inline std::ostream& operator<<(std::ostream& os, const PathMatcher::Error err) {
    switch (err) {
        case PathMatcher::Error::invalid_recursive_wildcard:
            os << "invalid_recursive_wildcard";
            break;
        case PathMatcher::Error::invalid_argument:
            os << "invalid_argument";
            break;
    }
    return os;
}

// Make PathMatcher::Error compatible with boost::system::result
BOOST_NORETURN BOOST_NOINLINE inline void
throw_exception_from_error(PathMatcher::Error const& e, boost::source_location const& loc) {
    boost::throw_with_location(std::runtime_error(fmt::format("PathMatcher::Error: {}", e)), loc);
}

}  // namespace rav

// Make PathMatcher::Error printable with fmt
template<>
struct fmt::formatter<rav::PathMatcher::Error>: ostream_formatter {};
