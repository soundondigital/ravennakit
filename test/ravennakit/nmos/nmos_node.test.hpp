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

#include "ravennakit/nmos/nmos_node.hpp"

namespace rav::nmos {

void test_nmos_node_configuration_json(const Node::Configuration& config, const boost::json::value& json);

}  // namespace rav::nmos
