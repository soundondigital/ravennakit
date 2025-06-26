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

#include "ravennakit/core/json.hpp"
#include "ravennakit/ravenna/ravenna_sender.hpp"

namespace rav {

void test_ravenna_sender_json(const RavennaSender& sender, const boost::json::value& json);

void test_ravenna_sender_destination_json(
    const RavennaSender::Destination& destination, const boost::json::value& json
);

void test_ravenna_sender_destinations_json(
    const std::vector<RavennaSender::Destination>& destinations, const boost::json::value& json
);

void test_ravenna_sender_configuration_json(const RavennaSender::Configuration& config, const boost::json::value& json);

}  // namespace rav