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

#include "nmos_receiver_audio.hpp"

#include <variant>
#include <boost/uuid/uuid.hpp>

namespace rav::nmos {

/**
 * Describes a Receiver.
 */
struct Receiver {
    std::variant<ReceiverAudio> any_of;

    [[nodiscard]] boost::uuids::uuid get_id() const {
        return std::visit(
            [](const auto& receiver_variant) {
                return receiver_variant.id;
            },
            any_of
        );
    }

    [[nodiscard]] boost::uuids::uuid get_device_id() const {
        return std::visit(
            [](const auto& receiver_variant) {
                return receiver_variant.device_id;
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

inline void tag_invoke(const boost::json::value_from_tag& tag, boost::json::value& jv, const Receiver& receiver) {
    std::visit(
        [&tag, &jv](const auto& receiver_variant) {
            tag_invoke(tag, jv, receiver_variant);
        },
        receiver.any_of
    );
}

}  // namespace rav::nmos