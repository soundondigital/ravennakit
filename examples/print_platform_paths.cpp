//
// Created by Ruurd Adema on 27/08/2025.
// Copyright (c) 2025 Sound on Digital. All rights reserved.
//

#include "ravennakit/core/file.hpp"
#include "ravennakit/core/util/paths.hpp"
#include "ravennakit/core/util/uri.hpp"

#include <fmt/format.h>

int main() {
    fmt::println("Home: file://{}", rav::Uri::encode(rav::paths::home().string()));
    fmt::println("Desktop: file://{}", rav::Uri::encode(rav::paths::desktop().string()));
    fmt::println("Documents: file://{}", rav::Uri::encode(rav::paths::documents().string()));
    fmt::println("Downloads: file://{}", rav::Uri::encode(rav::paths::downloads().string()));
    fmt::println("Pictures: file://{}", rav::Uri::encode(rav::paths::pictures().string()));
    fmt::println("Application data: file://{}", rav::Uri::encode(rav::paths::application_data().string()));
    fmt::println("Cache: file://{}", rav::Uri::encode(rav::paths::cache().string()));
}
