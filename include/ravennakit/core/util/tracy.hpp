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
#include "ravennakit/core/platform.hpp"

#if defined(TRACY_ENABLE) && TRACY_ENABLE
    #if RAV_APPLE
        #define TracyFunction __PRETTY_FUNCTION__
    #endif

BEGIN_IGNORE_WARNINGS
    #include <tracy/Tracy.hpp>
END_IGNORE_WARNINGS

    #define TRACY_ZONE_SCOPED ZoneScoped  // NOLINT(bugprone-reserved-identifier)
    #define TRACY_PLOT(name, value) TracyPlot(name, value)
    #define TRACY_MESSAGE(message) TracyMessageL(message)
    #define TRACY_MESSAGE_COLOR(message, color) TracyMessageLC(message, color)

#else
    #define TRACY_ZONE_SCOPED
    #define TRACY_PLOT(...)
    #define TRACY_MESSAGE(...)
#endif

namespace rav {

/**
 * Sometimes you want to know when something happened, but you don't want to instrument functions which get called into.
 * Then use this function to get a quick and dirty point in time.
 */
inline void tracy_point() {
    TRACY_ZONE_SCOPED;
}

}  // namespace rav
