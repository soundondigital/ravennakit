//
// Created by Ruurd Adema on 27/08/2025.
// Copyright (c) 2025 Sound on Digital. All rights reserved.
//

#pragma once

#include <filesystem>

namespace rav::paths {

/**
 * @return The path to the users home folder, or an empty path if the home folder could not be determined.
 */
std::filesystem::path home();

/**
 * @return The path to the desktop folder, or an empty path if the folder could not be determined.
 */
std::filesystem::path desktop();

/**
 * @return The path to the documents folder, or an empty path if the folder could not be determined.
 */
std::filesystem::path documents();

/**
 * @return The path to the pictures folder, or an empty path if the folder could not be determined.
 */
std::filesystem::path pictures();

/**
 * @return The path to the downloads folder, or an empty path if the folder could not be determined.
 */
std::filesystem::path downloads();

/**
 * @return A path to the application data folder, or an empty path if the folder could not be retrieved.
 */
std::filesystem::path application_data();

/**
 * @return A path to the cache folder, or an empty path if the folder could not be retrieved.
 */
std::filesystem::path cache();

}  // namespace rav::paths
