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

#include "ravennakit/core/platform.hpp"
#include "ravennakit/core/exception.hpp"
#include "ravennakit/core/log.hpp"

#if RAV_WINDOWS

    #include <synchapi.h>
    #include <handleapi.h>

namespace rav::windows {

/**
 * Wrapper around CreateEvent and CloseHandle.
 */
class event {
  public:
    /**
     * Constructs an event.
     * @throws rav::exception if CreateEvent fails.
     */
    event() {
        HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (event == nullptr) {
            RAV_THROW_EXCEPTION("Failed to create event");
        }
        event_ = event;
    }

    ~event() {
        if (event_ != nullptr) {
            if (CloseHandle(event_) == false) {
                RAV_ERROR("Failed to close event");
            }
        }
    }

    /**
     * @returns The underlying WSAEVENT.
     */
    [[nodiscard]] WSAEVENT get() const {
        return event_;
    }

    /**
     * Signals the event (SetEvent).
     * @throws rav::exception if SetEvent fails.
     */
    void signal() {
        if (!SetEvent(event_)) {
            RAV_THROW_EXCEPTION("Failed to signal event");
        }
    }

  private:
    WSAEVENT event_ {};
};

}  // namespace rav::windows

#endif
