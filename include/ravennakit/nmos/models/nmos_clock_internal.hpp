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
#include <string>
#include <boost/json/value.hpp>

namespace rav::nmos {

/**
 * Describes a clock with no external reference.
 */
struct ClockInternal {
    /// Name of this refclock (unique for this set of clocks). Must start with "clk".
    std::string name;

    /// Type of external reference used by this clock
    std::string ref_type {"internal"};
};

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const ClockInternal& clock) {
    jv = {{"name", clock.name}, {"ref_type", clock.ref_type}};
}

}  // namespace rav::nmos
