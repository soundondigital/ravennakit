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
#include <iostream>

#include "exception.hpp"
#include "log.hpp"

/**
 * When RAV_LOG_ON_ASSERT is defined as true (1), a log message will be emitted when an assertion is hit. Default is on.
 */
#ifndef RAV_LOG_ON_ASSERT
    #define RAV_LOG_ON_ASSERT 1  // Enabled by default
#endif

/**
 * When RAV_THROW_EXCEPTION_ON_ASSERT is defined as true (1), an exception will be thrown when an assertion is hit.
 * Default is off.
 */
#ifndef RAV_THROW_EXCEPTION_ON_ASSERT
    #define RAV_THROW_EXCEPTION_ON_ASSERT 0
#endif

/**
 * When RAV_ABORT_ON_ASSERT is defined as true (1), program execution will abort when an assertion is hit. Default is
 * off.
 */
#ifndef RAV_ABORT_ON_ASSERT
    #define RAV_ABORT_ON_ASSERT 0
#endif

/**
 * Logs a message if RAV_LOG_ON_ASSERT is set to true.
 * @param msg The message to log.
 */
#define LOG_IF_ENABLED(msg)  \
    if (RAV_LOG_ON_ASSERT) { \
        RAV_CRITICAL(msg);   \
    }

/**
 * Throws an exception if RAV_THROW_EXCEPTION_ON_ASSERT is set to true.
 * @param msg The message of the exception.
 */
#define THROW_EXCEPTION_IF_ENABLED(msg)  \
    if (RAV_THROW_EXCEPTION_ON_ASSERT) { \
        RAV_THROW_EXCEPTION(msg);        \
    }

/**
 * Aborts program execution if RAV_ABORT_ON_ASSERT is set to true.
 * @param msg The message to write to stdout before aborting.
 */
#define ABORT_IF_ENABLED(msg)                                      \
    if (RAV_ABORT_ON_ASSERT) {                                     \
        std::cerr << "Abort on assertion: " << (msg) << std::endl; \
        std::abort();                                              \
    }

/**
 * Assert condition to be true, otherwise:
 *  - Logs if enabled
 *  - Throws if enabled
 *  - Aborts if enabled
 * @param condition The condition to test.
 * @param message The message for logging, throwing and/or aborting.
 */
#define RAV_ASSERT(condition, message)                                \
    do {                                                              \
        if (!(condition)) {                                           \
            LOG_IF_ENABLED("Assertion failure: " message)             \
            THROW_EXCEPTION_IF_ENABLED("Assertion failure: " message) \
            ABORT_IF_ENABLED(message)                                 \
        }                                                             \
    } while (false)

/**
 * Assert condition to be true, otherwise:
 *  - Logs if enabled
 *  - Throws if enabled
 *  - Aborts if enabled
 *  - Returns void if `condition` is false
 * @param condition The condition to test.
 * @param message The message for logging, throwing and/or aborting.
 */
#define RAV_ASSERT_RETURN(condition, message)                         \
    do {                                                              \
        if (!(condition)) {                                           \
            LOG_IF_ENABLED("Assertion failure: " message)             \
            THROW_EXCEPTION_IF_ENABLED("Assertion failure: " message) \
            ABORT_IF_ENABLED(message)                                 \
            return;                                                   \
        }                                                             \
    } while (false)

/**
 * Assert condition to be true, otherwise:
 *  - Logs if enabled
 *  - Throws if enabled
 *  - Aborts if enabled
 *  - Returns given `return_value` if `condition` is false
 * @param condition The condition to test.
 * @param message The message for logging, throwing and/or aborting.
 * @param return_value The value to return.
 */
#define RAV_ASSERT_RETURN_WITH(condition, message, return_value)      \
    do {                                                              \
        if (!(condition)) {                                           \
            LOG_IF_ENABLED("Assertion failure: " message)             \
            THROW_EXCEPTION_IF_ENABLED("Assertion failure: " message) \
            ABORT_IF_ENABLED(message)                                 \
            return return_value;                                      \
        }                                                             \
    } while (false)

/**
 * Asserts given condition, but never throws. Useful for places where an exception cannot be thrown like destructors.
 * @param condition The condition to test.
 * @param message The message to log or abort with.
 */
#define RAV_ASSERT_NO_THROW(condition, message)           \
    do {                                                  \
        if (!(condition)) {                               \
            LOG_IF_ENABLED("Assertion failure: " message) \
            ABORT_IF_ENABLED(message)                     \
        }                                                 \
    } while (false)

/**
 * Asserts with false, entering the RAV_ASSERT procedure as a quick way to assert that a branch is invalid.
 * @param message The message to log, throw, abort with.
 */
#define RAV_ASSERT_FALSE(message) RAV_ASSERT(false, message)
