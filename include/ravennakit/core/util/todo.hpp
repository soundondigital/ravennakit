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

#include <iostream>

/**
 * This macro is used to mark a piece of code as a to do item.
 * It will print a message to the console and abort the program.
 * @param msg The message to print.
 */
#define TODO(msg)                                                                                \
    do {                                                                                         \
        std::cerr << "TODO: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        abort();                                                                                 \
    } while (0)
