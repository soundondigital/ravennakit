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

#include <cassert>

#include "exception.hpp"
#include "log.hpp"

// #define RAV_LOG_ON_ASSERT 1
// #define RAV_THROW_EXCEPTION_ON_ASSERT 1
// #define RAV_ABORT_ON_ASSERT 1

#ifndef RAV_LOG_ON_ASSERT
    #define RAV_LOG_ON_ASSERT 0
#endif

#ifndef RAV_THROW_EXCEPTION_ON_ASSERT
    #define RAV_THROW_EXCEPTION_ON_ASSERT 0
#endif

#ifndef RAV_ABORT_ON_ASSERT
    #define RAV_ABORT_ON_ASSERT 0
#endif

#define LOG_IF_ENABLED(msg)  \
    if (RAV_LOG_ON_ASSERT) { \
        RAV_CRITICAL(msg);   \
    }

#define THROW_EXCEPTION_IF_ENABLED(msg)  \
    if (RAV_THROW_EXCEPTION_ON_ASSERT) { \
        RAV_THROW_EXCEPTION(msg);        \
    }

#define ABORT_IF_ENABLED(msg)  \
    if (RAV_ABORT_ON_ASSERT) { \
        abort();               \
    }

#define RAV_ASSERT(condition, message)                                \
    do {                                                              \
        assert((condition) && (message));                             \
        if (!(condition)) {                                           \
            LOG_IF_ENABLED("Assertion failure: " message)             \
            THROW_EXCEPTION_IF_ENABLED("Assertion failure: " message) \
            ABORT_IF_ENABLED(message)                                 \
        }                                                             \
    } while (false)

#define RAV_ASSERT_FALSE(message) RAV_ASSERT(false, message)
