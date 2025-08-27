/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "log.hpp"
#include "assert.hpp"

#ifdef TL_ASSERT
    #error "TL_ASSERT is already defined. Please include this header before including <tl/expected.hpp>."
#else
    #define TL_ASSERT(condition) RAV_ASSERT(condition, "tl::expected assertion failed: " #condition)
#endif

#include <tl/expected.hpp>
