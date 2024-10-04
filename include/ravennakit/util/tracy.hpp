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

#include "ravennakit/core/warnings.hpp"

#if defined(TRACY_ENABLE) && TRACY_ENABLE
    #define TracyFunction __PRETTY_FUNCTION__

START_IGNORE_WARNINGS
    #include <tracy/Tracy.hpp>
END_IGNORE_WARNINGS

#else
    #define ZoneScoped
#endif
