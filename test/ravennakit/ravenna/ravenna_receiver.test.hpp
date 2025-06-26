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

#include "ravennakit/ravenna/ravenna_receiver.hpp"

namespace rav {

void test_ravenna_receiver_json(const RavennaReceiver& receiver, const boost::json::value& json);

void test_ravenna_receiver_configuration_json(
    const RavennaReceiver::Configuration& config, const boost::json::value& json
);

}  // namespace rav
