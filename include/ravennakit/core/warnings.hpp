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
    #define RAV_BEGIN_IGNORE_WARNINGS _Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wextra-semi\"") _Pragma("clang diagnostic ignored \"-Wundef\"") _Pragma(
    "clang diagnostic ignored \"-Wswitch-enum\""
)

#elif defined(__GNUC__) && (__GNUC__ >= 5)
    #define RAV_BEGIN_IGNORE_WARNINGS _Pragma("GCC diagnostic push")
#elif defined(_MSC_VER)
    #define RAV_BEGIN_IGNORE_WARNINGS _Pragma("warning (push, 0)")
#endif

#if defined(__clang__)
    #define RAV_END_IGNORE_WARNINGS _Pragma("clang diagnostic pop")
#elif defined(__GNUC__) && (__GNUC__ >= 5)
    #define RAV_END_IGNORE_WARNINGS _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
    #define RAV_END_IGNORE_WARNINGS _Pragma("warning (pop)")
#endif
