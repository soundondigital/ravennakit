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

#include <cstdlib>

#ifndef RAV_ENABLE_SPDLOG
    #define RAV_ENABLE_SPDLOG 0
#endif

#if RAV_ENABLE_SPDLOG

    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

    #if RAV_MACOS
        #define SPDLOG_FUNCTION __PRETTY_FUNCTION__
    #endif

    #include <spdlog/spdlog.h>

    #ifndef RAV_TRACE
        #define RAV_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
    #endif

    #ifndef RAV_DEBUG
        #define RAV_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
    #endif

    #ifndef RAV_CRITICAL
        #define RAV_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
    #endif

    #ifndef RAV_ERROR
        #define RAV_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
    #endif

    #ifndef RAV_WARNING
        #define RAV_WARNING(...) SPDLOG_WARN(__VA_ARGS__)
    #endif

    #ifndef RAV_INFO
        #define RAV_INFO(...) SPDLOG_INFO(__VA_ARGS__)
    #endif

    #ifndef RAV_LOG
        #define RAV_LOG(...) SPDLOG_INFO(__VA_ARGS__)
    #endif

#else

    #include <fmt/format.h>

    #ifndef RAV_TRACE
        #define RAV_TRACE(...) fmt::println(__VA_ARGS__)
    #endif

    #ifndef RAV_DEBUG
        #define RAV_DEBUG(...) fmt::println(__VA_ARGS__)
    #endif

    #ifndef RAV_CRITICAL
        #define RAV_CRITICAL(...) fmt::println(__VA_ARGS__)
    #endif

    #ifndef RAV_ERROR
        #define RAV_ERROR(...) fmt::println(__VA_ARGS__)
    #endif

    #ifndef RAV_WARNING
        #define RAV_WARNING(...) fmt::println(__VA_ARGS__)
    #endif

    #ifndef RAV_INFO
        #define RAV_INFO(...) fmt::println(__VA_ARGS__)
    #endif

    #ifndef RAV_LOG
        #define RAV_LOG(...) fmt::println(__VA_ARGS__)
    #endif

#endif

#define CATCH_LOG_UNCAUGHT_EXCEPTIONS                                                                          \
    catch (const rav::exception& e) {                                                                          \
        RAV_CRITICAL(                                                                                          \
            "rav::exception caught: {} - please handle your exceptions before reaching this point.", e.what()  \
        );                                                                                                     \
    }                                                                                                          \
    catch (const std::exception& e) {                                                                          \
        RAV_CRITICAL(                                                                                          \
            "std::exception caucght: {} - please handle your exceptions before reaching this point.", e.what() \
        );                                                                                                     \
    }                                                                                                          \
    catch (...) {                                                                                              \
        RAV_CRITICAL("unknown exception caucght - please handle your exceptions before reaching this point."); \
    }

namespace rav::log {

/**
 * Tries to find given environment variable and set the log level accordingly.
 * The following are valid values for the log level:
 *  - TRACE
 *  - DEBUG
 *  - INFO (default)
 *  - WARN
 *  - ERROR
 *  - CRITICAL
 *  - OFF
 * By default the log level is set to INFO.
 * Note: setting the log level is currently only implemented for spdlog.
 * TODO: Implement log level setting for fmt.
 * @param env_var The environment variable to read the log level from.
 */
inline void set_level_from_env(const char* env_var = "RAV_LOG_LEVEL") {
    if (const auto* env_value = std::getenv(env_var)) {
        const std::string_view level(env_value);
#if RAV_ENABLE_SPDLOG
        if (level == "TRACE") {
            spdlog::set_level(spdlog::level::trace);
        } else if (level == "DEBUG") {
            spdlog::set_level(spdlog::level::debug);
        } else if (level == "INFO") {
            spdlog::set_level(spdlog::level::info);
        } else if (level == "WARN") {
            spdlog::set_level(spdlog::level::warn);
        } else if (level == "ERROR") {
            spdlog::set_level(spdlog::level::err);
        } else if (level == "CRITICAL") {
            spdlog::set_level(spdlog::level::critical);
        } else if (level == "CRITICAL") {
            spdlog::set_level(spdlog::level::off);
        }
#endif
    } else {
#if RAV_ENABLE_SPDLOG
        spdlog::set_level(spdlog::level::info);
#endif
    }
}
}  // namespace rav::log
