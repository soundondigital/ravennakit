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

namespace rav::nmos {

struct Activation {
    enum class Mode { activate_immediate, activate_scheduled_absolute, activate_scheduled_relative };

    /**
     * Mode of activation: immediate (on message receipt), scheduled_absolute (when internal clock >= requested_time),
     * scheduled_relative (when internal clock >= time of message receipt + requested_time), or null (no activation
     * scheduled). This parameter returns to null on the staged endpoint once an activation is completed or when it is
     * explicitly set to null. For immediate activations, in the response to the PATCH request this field will be set to
     * 'activate_immediate', but will be null in response to any subsequent GET requests.
     */
    std::optional<Mode> mode;

    /**
     * String formatted TAI timestamp (<seconds>:<nanoseconds>) indicating time (absolute or relative) for activation
     * requested. This field returns to null once the activation is completed on the staged endpoint or when the
     * resource is unlocked by setting the activation mode to null. For an immediate activation this field will always
     * be null on the staged endpoint, even in the response to the PATCH request.
     */
    std::optional<Timestamp> requested_time;
};

inline const char* to_string(const Activation::Mode mode) {
    switch (mode) {
        case Activation::Mode::activate_immediate:
            return "activate_immediate";
        case Activation::Mode::activate_scheduled_absolute:
            return "activate_scheduled_absolute";
        case Activation::Mode::activate_scheduled_relative:
            return "activate_scheduled_relative";
    }
    return "";
}

inline void
tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Activation::Mode& mode) {
    jv = to_string(mode);
}

inline Activation::Mode
tag_invoke(const boost::json::value_to_tag<Activation::Mode>&, const boost::json::value& jv) {
    const auto str = jv.as_string();
    if (str == "activate_immediate") {
        return Activation::Mode::activate_immediate;
    }
    if (str == "activate_scheduled_absolute") {
        return Activation::Mode::activate_scheduled_absolute;
    }
    if (str == "activate_scheduled_relative") {
        return Activation::Mode::activate_scheduled_relative;
    }
    throw std::runtime_error("Unknown activation mode tag");
}

inline void
tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Activation& activation) {
    jv = {
        {"mode", boost::json::value_from(activation.mode)},
        {"requested_time", boost::json::value_from(activation.requested_time)},
    };
}

inline Activation
tag_invoke(const boost::json::value_to_tag<Activation>&, const boost::json::value& jv) {
    Activation act;
    if (const auto result = jv.try_at("mode")) {
        act.mode = boost::json::value_to<Activation::Mode>(*result);
    }
    if (const auto result = jv.try_at("requested_time")) {
        act.requested_time = boost::json::value_to<Timestamp>(*result);
    }
    return act;
}

}
