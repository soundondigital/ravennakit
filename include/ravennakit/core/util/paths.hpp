//
// Created by Ruurd Adema on 27/08/2025.
// Copyright (c) 2025 Sound on Digital. All rights reserved.
//

#pragma once

#include <filesystem>

namespace rav::paths {

/**
 *
 * @return A path to the users home folder, or an empty path if the home folder could not be determined.
 * On macOS this returns: /Users/<username>
 */
std::filesystem::path home();

/**
 *
 * @return A path to the application data folder, or an empty path if the folder could not be retrieved.
 * On macOS this returns: /Users/<username>/Library/Application Support
 */
std::filesystem::path application_data();

}  // namespace rav::paths
