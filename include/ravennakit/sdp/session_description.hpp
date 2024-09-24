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

#include <sstream>

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/string.hpp"
#include "ravennakit/core/util.hpp"

namespace rav {

class session_description {
  public:
    enum class parse_result { ok, invalid_version, invalid_line };

    static std::pair<parse_result, session_description> parse(const std::string& sdp_text) {
        session_description sd;
        std::istringstream stream(sdp_text);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) {
                continue;
            }

            if (line.back() == '\r') {  // Remove trailing '\r' if present (because of CRLF)
                line.pop_back();
            }

            if (line.size() < 2) {
                RAV_ERROR("Invalid line: {}", line);
                return {parse_result::invalid_line, {}};
            }

            switch (line.front()) {
                case 'v':
                    if (auto result = sd.parse_version(line); result != parse_result::ok) {
                        return {result, {}};
                    }
                    break;
                case 'o':
                    if (auto result = sd.parse_origin(line); result != parse_result::ok) {
                        return {result, {}};
                    }
                    break;
                default:
                    continue;
            }
        }

        return {parse_result::ok, sd};
    }

    [[nodiscard]] int version() const {
        return version_;
    }

  private:
    int version_ {};

    parse_result parse_version(const std::string_view line) {
        const auto value = from_first_occurrence_of(line, "=", false);
        const auto v = rav::from_string_strict<int>(value);
        if (!v) {
            return parse_result::invalid_version;
        }
        version_ = *v;
        return parse_result::ok;
    }

    parse_result parse_origin(const std::string_view line) {
        const auto value = from_first_occurrence_of(line, "=", false);

        return parse_result::ok;
    }
};

}  // namespace rav
