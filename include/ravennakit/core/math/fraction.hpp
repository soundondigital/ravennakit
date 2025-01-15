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

namespace rav {

/**
 * Little tiny struct to represent a fraction without loss.
 * @tparam T The type of the numerator and denominator.
 */
template<class T>
struct fraction {
    T numerator;
    T denominator;
};

}
