//
// Created by Ruurd Adema on 27/08/2025.
// Copyright (c) 2025 Sound on Digital. All rights reserved.
//

#include "ravennakit/core/env.hpp"
#include "ravennakit/core/util/paths.hpp"

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/file.hpp"
#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/core/util/defer.hpp"

#include <filesystem>

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

#if RAV_WINDOWS

    #include <winerror.h>
    #include <stringapiset.h>
    #include <ShlObj.h>

namespace {

std::filesystem::path get_known_windows_folder(REFKNOWNFOLDERID folderId) {
    LPWSTR wsz_path = nullptr;
    HRESULT hr;
    hr = SHGetKnownFolderPath(folderId, KF_FLAG_CREATE, nullptr, &wsz_path);

    rav::Defer free_ptr([wsz_path] {
        CoTaskMemFree(wsz_path);
    });

    if (!SUCCEEDED(hr)) {
        return {};
    }

    return wsz_path;
}

std::filesystem::path windows_get_roaming_app_data() {
    return get_known_windows_folder(FOLDERID_RoamingAppData);
}

std::filesystem::path windows_get_program_data() {
    return get_known_windows_folder(FOLDERID_ProgramData);
}

std::filesystem::path windows_get_local_app_data() {
    return get_known_windows_folder(FOLDERID_LocalAppData);
}

}  // namespace

#endif

#if RAV_LINUX

inline std::filesystem::path resolve_xdg_folder(const char* type, std::filesystem::path fallback_path) {
    const auto home = get_home();
    const auto result = rav::file::read_file_as_string(home / ".config" / "user-dirs.dirs");
    if (!result) {
        return fallback_path;
    }

    rav::StringParser line_parser(*result);
    while (auto line = line_parser.read_line()) {
        const auto trimmed = rav::string_trim(*line);
        if (rav::string_starts_with(trimmed, type)) {
            const auto path = rav::string_from_first_occurrence_of(trimmed, "=", false);

            // eg. resolve XDG_MUSIC_DIR="$HOME/Music" to /home/user/Music
            auto replaced = rav::string_replace(path, "$HOME", home.c_str());
            auto unquoted = rav::string_unquoted(replaced);
            if (std::filesystem::is_directory(unquoted)) {
                return unquoted;
            }
        }
    }

    return fallback_path;
}

#endif

std::filesystem::path rav::paths::home() {
#if RAV_POSIX
    return get_home();
#elif RAV_WINDOWS
    return get_known_windows_folder(FOLDERID_Profile);
#else
    RAV_ASSERT_FALSE("Platform not supported");
    return {};
#endif
}

std::filesystem::path rav::paths::desktop() {
#if RAV_MACOS
    const auto home = get_home();
    if (home.empty()) {
        return {};
    }
    return home / "Desktop";
#elif RAV_WINDOWS
    return get_known_windows_folder(FOLDERID_Desktop);
#elif RAV_LINUX
    return resolve_xdg_folder("XDG_DESKTOP_DIR", get_home() / "Desktop");
#else
    RAV_ASSERT_FALSE("Platform not supported");
    return {};
#endif
}

std::filesystem::path rav::paths::documents() {
#if RAV_MACOS
    const auto home = get_home();
    if (home.empty()) {
        return {};
    }
    return home / "Documents";
#elif RAV_WINDOWS
    return get_known_windows_folder(FOLDERID_Documents);
#elif RAV_LINUX
    return resolve_xdg_folder("XDG_DOCUMENTS_DIR", get_home() / "Documents");
#else
    RAV_ASSERT_FALSE("Platform not supported");
    return {};
#endif
}

std::filesystem::path rav::paths::pictures() {
#if RAV_MACOS
    const auto home = get_home();
    if (home.empty()) {
        return {};
    }
    return home / "Pictures";
#elif RAV_WINDOWS
    return get_known_windows_folder(FOLDERID_Pictures);
#elif RAV_LINUX
    return resolve_xdg_folder("XDG_PICTURES_DIR", get_home() / "Pictures");
#else
    RAV_ASSERT_FALSE("Platform not supported");
    return {};
#endif
}

std::filesystem::path rav::paths::downloads() {
#if RAV_MACOS
    const auto home = get_home();
    if (home.empty()) {
        return {};
    }
    return home / "Downloads";
#elif RAV_WINDOWS
    return get_known_windows_folder(FOLDERID_Downloads);
#elif RAV_LINUX
    return resolve_xdg_folder("XDG_DOWNLOAD_DIR", get_home() / "Downloads");
#else
    RAV_ASSERT_FALSE("Platform not supported");
    return {};
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
    return windows_get_roaming_app_data();
#elif RAV_LINUX
    return resolve_xdg_folder("XDG_CONFIG_HOME", get_home() / ".local" / "share");
#else
    RAV_ASSERT_FALSE("Platform not supported");
    return {};
#endif
}

std::filesystem::path rav::paths::cache() {
#if RAV_MACOS
    const auto home = get_home();
    if (home.empty()) {
        return {};
    }
    return home / "Library" / "Caches";
#elif RAV_WINDOWS
    return windows_get_local_app_data();
#elif RAV_LINUX
    return resolve_xdg_folder("XDG_CACHE_HOME", get_home() / ".cache");
#else
    RAV_ASSERT_FALSE("Platform not supported");
    return {};
#endif
}
