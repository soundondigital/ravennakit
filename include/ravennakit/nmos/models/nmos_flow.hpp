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
#include "nmos_flow_audio_raw.hpp"

namespace rav::nmos {

/**
 * Flow is a generic structure that can hold different types of flows.
 */
struct Flow {
    std::variant<FlowAudioRaw> any_of;

    [[nodiscard]] boost::uuids::uuid id() const {
        return std::visit(
            [](const auto& flow_variant) {
                return flow_variant.id;
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

    [[nodiscard]] boost::uuids::uuid get_device_id() const {
        return std::visit(
            [](const auto& flow_variant) {
                return flow_variant.device_id;
            },
            any_of
        );
    }
};

inline void tag_invoke(const boost::json::value_from_tag& tag, boost::json::value& jv, const Flow& flow) {
    std::visit(
        [&tag, &jv](const auto& flow_variant) {
            tag_invoke(tag, jv, flow_variant);
        },
        flow.any_of
    );
}

}  // namespace rav::nmos
