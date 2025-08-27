//
// Created by Ruurd Adema on 27/08/2025.
// Copyright (c) 2025 Sound on Digital. All rights reserved.
//

#include "ravennakit/core/env.hpp"
#include "ravennakit/core/util/paths.hpp"

#include <filesystem>
#include <string>
#include <vector>

#if RAV_POSIX

    #include <unistd.h>
    #include <pwd.h>

namespace {

/**
 * Retrieves the effective user's home dir.
 * If the user is running as root we ignore the HOME environment. It works badly with sudo.
 * @return The home directory. HOME environment is respected for non-root users if it exists.
 */
std::filesystem::path get_home() {
    std::string res;
    const auto uid = getuid();

    if (const char* home_env = std::getenv("HOME"); home_env != nullptr && uid != 0) {
        return home_env;  // We only acknowledge HOME if not root.
    }

    passwd* pw = nullptr;
    passwd pwd {};
    long buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (buf_size < 1) {
        buf_size = 16384;
    }
    std::vector<char> buffer;
    buffer.resize(static_cast<size_t>(buf_size));
    int error_code = getpwuid_r(uid, &pwd, buffer.data(), buffer.size(), &pw);

    for (int i = 0; i < 128 && error_code == ERANGE; i++) {
        // The buffer was too small. Try again with a larger buffer.
        buf_size *= 2;
        buffer.resize(static_cast<size_t>(buf_size));
        error_code = getpwuid_r(uid, &pwd, buffer.data(), buffer.size(), &pw);
    }

    if (error_code != 0) {
        return {};
    }

    if (pw->pw_dir == nullptr) {
        return {};  // User has no home directory
    }

    return pw->pw_dir;
}

}  // namespace

#endif

std::filesystem::path rav::paths::home() {
#if RAV_POSIX
    return get_home();
#else
    return {}
#endif
}

std::filesystem::path rav::paths::application_data() {
#if RAV_MACOS
    const auto home = get_home();
    if (home.empty()) {
        return {};
    }
    return home / "Library" / "Application Support";
#elif RAV_WINDOWS

#endif
}
