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

#if defined(TRACY_ENABLE) && TRACY_ENABLE
    #define TracyFunction __PRETTY_FUNCTION__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wextra-semi"
    #include <tracy/Tracy.hpp>
    #pragma GCC diagnostic pop
#else
    #define ZoneScoped
#endif
