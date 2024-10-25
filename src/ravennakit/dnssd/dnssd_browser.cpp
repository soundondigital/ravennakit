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
#include "ravennakit/dnssd/dnssd_browser.hpp"
#include "ravennakit/dnssd/bonjour/bonjour_browser.hpp"

std::unique_ptr<rav::dnssd::dnssd_browser> rav::dnssd::dnssd_browser::create(asio::io_context& io_context) {
#if RAV_APPLE
    return std::make_unique<bonjour_browser>(io_context);
#elif RAV_WINDOWS
    if (dnssd::is_bonjour_service_running()) {
        return std::make_unique<bonjour_browser>();
    } else {
        return {};
    }
#else
    return {};
#endif
}
