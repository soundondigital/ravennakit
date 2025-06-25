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

#if RAV_WINDOWS

    #include <winsock2.h>
    #include <mswsock.h>

typedef BOOL(PASCAL* LPFN_WSARECVMSG)(
    SOCKET s, LPWSAMSG lpMsg, LPDWORD lpNumberOfBytesRecvd, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

namespace rav::windows {

/**
 * A wrapper around the WSARecvMsg function which is retrieved dynamically at runtime.
 */
class wsa_recv_msg_function {
  public:
    /**
     * Constructor which retrieves the WSARecvMsg function.
     */
    wsa_recv_msg_function() {
        SOCKET temp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        rav::Defer defer_closing_socket([&temp_sock] {
            closesocket(temp_sock);
        });
        DWORD bytes_returned = 0;
        GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;

        if (WSAIoctl(
                temp_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &WSARecvMsg_GUID, sizeof(WSARecvMsg_GUID),
                &wsa_recv_msg_func_, sizeof(wsa_recv_msg_func_), &bytes_returned, nullptr, nullptr
            )
            == SOCKET_ERROR) {
            RAV_THROW_EXCEPTION(fmt::format("Failed to get WSARecvMsg function: {}", WSAGetLastError()));
        }
    }

    /**
     * Get the WSARecvMsg function.
     * @return The WSARecvMsg function.
     */
    [[nodiscard]] LPFN_WSARECVMSG get() const {
        return wsa_recv_msg_func_;
    }

    /**
     * Get the global instance of the WSARecvMsg function.
     * @throws std::exception If the function could not be retrieved.
     * @return The global instance of the WSARecvMsg function.
     */
    static LPFN_WSARECVMSG get_global() {
        static wsa_recv_msg_function instance;
        return instance.wsa_recv_msg_func_;
    }

  private:
    LPFN_WSARECVMSG wsa_recv_msg_func_ {};
};

}  // namespace rav

#endif
