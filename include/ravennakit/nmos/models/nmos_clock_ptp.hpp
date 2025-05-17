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
#include <boost/json/conversion.hpp>

namespace rav::nmos {

/**
 * Describes a clock referenced to PTP.
 */
struct ClockPtp {
    /// Name of this refclock (unique for this set of clocks). Must start with "clk".
    std::string name;

    /// Type of external reference used by this clock
    std::string ref_type {"ptp"};  // The only value in v1.3

    /// External refclock is synchronized to International Atomic Time (TAI)
    bool traceable {false};

    /// Version of PTP reference used by this clock
    std::string version {"IEEE1588-2008"};  // The only value in v1.3

    /// ID of the PTP reference used by this clock (e.g. "00-1a-2b-00-00-3c-4d-5e")
    std::string gmid;

    /// Lock-state of this clock to the external reference. If true, this device follows the external reference;
    /// otherwise it has no defined relationship to the external reference
    bool locked {false};
};

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const ClockPtp& clock) {
    jv = {{"name", clock.name},       {"ref_type", clock.ref_type}, {"traceable", clock.traceable},
          {"version", clock.version}, {"gmid", clock.gmid},         {"locked", clock.locked}};
}

}  // namespace rav::nmos
