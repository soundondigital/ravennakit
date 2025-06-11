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

#include "nmos_source_audio.hpp"

#include <variant>

namespace rav::nmos {

/**
 * Describes a Source
 * https://specs.amwa.tv/is-04/releases/v1.3.3/APIs/schemas/with-refs/source.html
 */
struct Source {
    std::variant<SourceAudio> any_of;

    [[nodiscard]] boost::uuids::uuid get_id() const {
        return std::visit(
            [](const auto& source) {
                return source.id;
            },
            any_of
        );
    }

    [[nodiscard]] Version get_version() const {
        return std::visit(
            [](const auto& source) {
                return source.version;
            },
            any_of
        );
    }

    void set_version(const Version& version) {
        std::visit(
            [&version](auto& source) {
                source.version = version;
            },
            any_of
        );
    }
};

inline void tag_invoke(const boost::json::value_from_tag& tag, boost::json::value& jv, const Source& source) {
    std::visit(
        [&tag, &jv](const auto& s) {
            tag_invoke(tag, jv, s);
        },
        source.any_of
    );
}

}  // namespace rav::nmos
