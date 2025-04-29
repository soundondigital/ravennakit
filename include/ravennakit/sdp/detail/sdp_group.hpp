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

#include "ravennakit/core/expected.hpp"

#include <string>

namespace rav::sdp {

/**
 * Represents a group of media descriptions in an SDP message.
 * RFC 5888: https://datatracker.ietf.org/doc/html/rfc5888
 * RFC 7104: https://datatracker.ietf.org/doc/html/rfc7104
 */
class Group {
  public:
    enum class Type {
        undefined = 0,
        dup,
    };

    /**
     * @return The type of the group. Only DUP is currently supported.
     */
    [[nodiscard]] Type get_type() const;

    /**
     * @param type The type of the group.
     */
    void set_type(Type type);

    /**
     * @return The tags associated with the group.
     */
    [[nodiscard]] const std::vector<std::string>& get_tags() const;

    /**
     * Gets one of the tags.
     * @param index The index of the tag to retrieve.
     * @return The tag at the specified index, or an empty string if the index is out of bounds.
     */
    [[nodiscard]] const std::string& get_tag(size_t index) const;

    /**
     * Adds a tag to the group.
     * @param tag The tag to add.
     */
    void add_tag(std::string tag);

    /**
     * @return The group encoded as string.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string() const;

    /**
     * @return True if the group is empty (no tags), false otherwise.
     */
    [[nodiscard]] bool empty() const;

    /**
     * Parses a group line from an SDP message.
     * @param line The line to parse.
     * @return A tl::expected object containing the parsed group or an error message.
     */
    static tl::expected<Group, std::string> parse_new(std::string_view line);

  private:
    Type type_ = Type::undefined;
    std::vector<std::string> tags_;
};

}  // namespace rav::sdp
