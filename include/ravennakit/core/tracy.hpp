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
    #if RAV_APPLE
        #define TracyFunction __PRETTY_FUNCTION__
    #endif

START_IGNORE_WARNINGS
    #include <tracy/Tracy.hpp>
END_IGNORE_WARNINGS

    #define TRACY_ZONE_SCOPED ZoneScoped  // NOLINT(bugprone-reserved-identifier)
    #define TRACY_PLOT(name, value) TracyPlot(name, value)

#else
    #define TRACY_ZONE_SCOPED
    #define TRACY_PLOT
#endif
