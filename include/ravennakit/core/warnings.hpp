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

#if defined(__clang__)
    #define START_IGNORE_WARNINGS _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wextra-semi\"")
#elif defined(__GNUC__) && (__GNUC__ >= 5)
    #define START_IGNORE_WARNINGS _Pragma("GCC diagnostic push")
#elif defined(_MSC_VER)
    #define START_IGNORE_WARNINGS _Pragma("warning (push, 0)")
#endif

#if defined(__clang__)
    #define END_IGNORE_WARNINGS _Pragma("clang diagnostic pop")
#elif defined(__GNUC__) && (__GNUC__ >= 5)
    #define END_IGNORE_WARNINGS _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
    #define END_IGNORE_WARNINGS _Pragma("warning (pop)")
#endif
