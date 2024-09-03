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

#ifdef RAV_ENABLE_SPDLOG

    #include <spdlog/spdlog.h>

    #ifdef RAV_TRACE
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

    #ifdef RAV_TRACE
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

/**
 * Define the function name macro if it is not already defined.
 */
#ifndef RAV_FUNCTION
    #define RAV_FUNCTION static_cast<const char*>(__FUNCTION__)
#endif
