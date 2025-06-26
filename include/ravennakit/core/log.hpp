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
#include "env.hpp"
#include "string.hpp"

#include <atomic>
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

    #include "ravennakit/core/format.hpp"

enum class LogLevel { off, critical, error, warning, info, debug, trace };

inline std::atomic log_level = LogLevel::info;

    #ifndef RAV_TRACE
        #define RAV_TRACE(...)                         \
            if (log_level.load() >= LogLevel::trace) { \
                fmt::println("[T] " __VA_ARGS__);      \
            }
    #endif

    #ifndef RAV_DEBUG
        #define RAV_DEBUG(...)                         \
            if (log_level.load() >= LogLevel::debug) { \
                fmt::println("[D] " __VA_ARGS__);      \
            }
    #endif

    #ifndef RAV_CRITICAL
        #define RAV_CRITICAL(...)                         \
            if (log_level.load() >= LogLevel::critical) { \
                fmt::println("[C] " __VA_ARGS__);         \
            }
    #endif

    #ifndef RAV_ERROR
        #define RAV_ERROR(...)                         \
            if (log_level.load() >= LogLevel::error) { \
                fmt::println("[E] " __VA_ARGS__);      \
            }
    #endif

    #ifndef RAV_WARNING
        #define RAV_WARNING(...)                         \
            if (log_level.load() >= LogLevel::warning) { \
                fmt::println("[W] " __VA_ARGS__);        \
            }
    #endif

    #ifndef RAV_INFO
        #define RAV_INFO(...)                         \
            if (log_level.load() >= LogLevel::info) { \
                fmt::println("[I] " __VA_ARGS__);     \
            }
    #endif

    #ifndef RAV_LOG
        #define RAV_LOG(...)                           \
            if (log_level.load() >= LogLevel::trace) { \
                fmt::println("[L] " __VA_ARGS__);      \
            }
    #endif

#endif

#define CATCH_LOG_UNCAUGHT_EXCEPTIONS                                                                          \
    catch (const rav::Exception& e) {                                                                          \
        RAV_CRITICAL(                                                                                          \
            "rav::Exception caught: {} - please handle your exceptions before reaching this point.", e.what()  \
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

namespace rav {

/**
 * Sets the log level for the application based on the given string.
 * The following are valid values:
 *  - TRACE
 *  - DEBUG
 *  - INFO (default)
 *  - WARN
 *  - ERROR
 *  - CRITICAL
 *  - OFF
 * @param level The log level as string, case-insensitive.
 */
inline void set_log_level(const char* level) {
#if RAV_ENABLE_SPDLOG
    if (string_compare_case_insensitive(level, "TRACE")) {
        spdlog::set_level(spdlog::level::trace);
    } else if (string_compare_case_insensitive(level, "DEBUG")) {
        spdlog::set_level(spdlog::level::debug);
    } else if (string_compare_case_insensitive(level, "INFO")) {
        spdlog::set_level(spdlog::level::info);
    } else if (string_compare_case_insensitive(level, "WARN")) {
        spdlog::set_level(spdlog::level::warn);
    } else if (string_compare_case_insensitive(level, "ERROR")) {
        spdlog::set_level(spdlog::level::err);
    } else if (string_compare_case_insensitive(level, "CRITICAL")) {
        spdlog::set_level(spdlog::level::critical);
    } else if (string_compare_case_insensitive(level, "OFF")) {
        spdlog::set_level(spdlog::level::off);
    } else {
        fmt::println("Invalid log level: {}. Setting log level to info.", level);
        spdlog::set_level(spdlog::level::info);
    }
#else
    if (string_compare_case_insensitive(level, "TRACE")) {
        log_level = LogLevel::trace;
    } else if (string_compare_case_insensitive(level, "DEBUG")) {
        log_level = LogLevel::debug;
    } else if (string_compare_case_insensitive(level, "INFO")) {
        log_level = LogLevel::info;
    } else if (string_compare_case_insensitive(level, "WARN")) {
        log_level = LogLevel::warning;
    } else if (string_compare_case_insensitive(level, "ERROR")) {
        log_level = LogLevel::error;
    } else if (string_compare_case_insensitive(level, "CRITICAL")) {
        log_level = LogLevel::critical;
    } else if (string_compare_case_insensitive(level, "OFF")) {
        log_level = LogLevel::off;
    } else {
        fmt::println("Invalid log level: {}. Setting log level to info.", level);
        log_level = LogLevel::info;
    }
#endif
}

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
 * If an invalid loglevel is passed, the loglevel will be set to INFO.
 * @param env_var The environment variable to read the log level from.
 */
inline void set_log_level_from_env(const char* env_var = "RAV_LOG_LEVEL") {
    if (const auto env_value = get_env(env_var)) {
        set_log_level(env_value->c_str());
    } else {
        set_log_level("INFO");
    }
}

}  // namespace rav
