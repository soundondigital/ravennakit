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

#include <string_view>

namespace rav {

/**
 * A simple path matcher that matches paths against patterns.
 * The pattern can contain wildcards, such as "**" for any path.
 * At this moment this class is more a skeleton for future development, and only supports a catch-all through "**" or
 * exact match.
 */
class PathMatcher {
  public:
    [[nodiscard]] static bool match(const std::string_view path, const std::string_view pattern) {
        if (pattern == "**") {
            return true;
        }
        return path == pattern;
    }
};

}  // namespace rav
