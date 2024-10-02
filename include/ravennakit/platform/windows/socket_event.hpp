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

#include "ravennakit/core/exception.hpp"
#include "ravennakit/core/platform.hpp"

#if RAV_WINDOWS

    #include <winsock2.h>

namespace rav::windows {

/**
 * Wrapper around WSACreateEvent and WSACloseEvent.
 */
class socket_event {
  public:
    /**
     * Constructs a socket event.
     * @throws rav::exception if WSACreateEvent fails.
     */
    socket_event() {
        event_ = WSACreateEvent();
        if (event_ == WSA_INVALID_EVENT) {
            RAV_THROW_EXCEPTION("WSACreateEvent failed");
        }
    }

    virtual ~socket_event() {
        if (event_ != WSA_INVALID_EVENT) {
            if (WSACloseEvent(event_) == false) {
                RAV_ERROR("WSACloseEvent failed");
            }
        }
    }

    /**
     *
     * @returns The underlying WSAEVENT.
     */
    [[nodiscard]] WSAEVENT get() const {
        return event_;
    }

    /**
     * Resets the event (WSAResetEvent).
     */
    void reset_event() {
        if (WSAResetEvent(event_) == SOCKET_ERROR) {
            RAV_THROW_EXCEPTION("WSAResetEvent failed");
        }
    }

    /**
     * Associates the event with a socket (WSAEventSelect).
     * @param socket The socket to associate with.
     */
    void associate(SOCKET socket) {
        if (WSAEventSelect(socket, event_, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR) {
            RAV_THROW_EXCEPTION("WSAEventSelect failed");
        }
    }

  private:
    WSAEVENT event_;
};

}  // namespace rav::windows

#endif
