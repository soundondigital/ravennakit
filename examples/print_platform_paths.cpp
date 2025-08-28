//
// Created by Ruurd Adema on 27/08/2025.
// Copyright (c) 2025 Sound on Digital. All rights reserved.
//

#include "ravennakit/core/file.hpp"
#include "ravennakit/core/util/paths.hpp"
#include "ravennakit/core/util/uri.hpp"

#include <fmt/format.h>

namespace {
void print(const char* subject, const std::filesystem::path& path) {
#if RAV_WINDOWS
    fmt::println("{}: {}", subject, path.string());
#else
    fmt::println("{}: file://{}", subject, rav::Uri::encode(path.string()));
#endif
}
}  // namespace

int main() {
    print("Home", rav::paths::home());
    print("Desktop", rav::paths::desktop());
    print("Documents", rav::paths::documents());
    print("Downloads", rav::paths::downloads());
    print("Pictures", rav::paths::pictures());
    print("Application data", rav::paths::application_data());
    print("Cache", rav::paths::cache());
    print("Temporary std", std::filesystem::temp_directory_path());
}
