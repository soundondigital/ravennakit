//
// Created by Ruurd Adema on 27/08/2025.
// Copyright (c) 2025 Sound on Digital. All rights reserved.
//

#include "ravennakit/core/util/paths.hpp"

#include <fmt/format.h>

int main() {
    fmt::println("Home: {}", rav::paths::home().c_str());
    fmt::println("Application data: {}", rav::paths::application_data().c_str());
}
