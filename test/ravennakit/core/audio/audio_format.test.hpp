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

#include "ravennakit/core/audio/audio_format.hpp"

namespace rav {

void test_audio_format_json(const AudioFormat& audio_format, const boost::json::value& json);

}  // namespace rav
