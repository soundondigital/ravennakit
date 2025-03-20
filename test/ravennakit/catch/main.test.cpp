/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <catch2/catch_session.hpp>

#include "ravennakit/core/log.hpp"

int main(const int argc, char* argv[]) {
    rav::set_log_level_from_env();
    return Catch::Session().run(argc, argv);
}
