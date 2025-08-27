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

#include "platform.hpp"

#if RAV_WINDOWS
    #include <windows.h>
    #include <processenv.h>
#endif

#include <string>
#include <optional>

namespace rav {

/**
 * Gets the value of an environment variable.
 * @param name The name of the variable to retrieve. Pointer must be non-null and \0 terminated.
 * @return The environment variable value, or an empty optional if the variable was not found.
 */
inline std::optional<std::string> get_env(const char* name) {
#if RAV_WINDOWS
    std::string value(64, 0);
    auto length = GetEnvironmentVariableA(name, value.data(), static_cast<DWORD>(value.size()));
    if (length == 0) {
        return {};
    }
    if (length > value.size() - 1) {
        value.resize(length, 0);
        length = GetEnvironmentVariableA(name, value.data(), static_cast<DWORD>(value.size()));
        assert(length == value.size() - 1);
    }
    value.resize(length);
    return value;
#else
    if (auto* value = std::getenv(name)) {
        return value;
    }
    return {};
#endif
}

}  // namespace rav::env
