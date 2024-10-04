/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/platform.hpp"

#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/dnssd/bonjour/bonjour_advertiser.hpp"

std::unique_ptr<rav::dnssd::dnssd_advertiser> rav::dnssd::dnssd_advertiser::create() {
#if RAV_APPLE
    return std::make_unique<bonjour_advertiser>();
#elif RAV_WINDOWS
    if (dnssd::is_bonjour_service_running()) {
        return std::make_unique<bonjour_advertiser>();
    } else {
        return {};
    }
#else
    return {};
#endif
}
