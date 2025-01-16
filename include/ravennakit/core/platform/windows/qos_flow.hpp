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

#include "ravennakit/core/platform.hpp"
#include "ravennakit/core/expected.hpp"
#include <asio.hpp>

#if RAV_WINDOWS

    #include <qos2.h>
    #pragma comment(lib, "qwave.lib")

namespace rav {

/**
 * A wrapper around Window's tedious QOS api... As alternative to IP_TOS.
 * You'll need administrator privileges for this.
 */
class qos_flow {
  public:
    qos_flow() {
        if (!QOSCreateHandle(&version_, &qos_handle_)) {
            RAV_THROW_EXCEPTION("Failed to create QOS handle: {}", GetLastError());
        }
    }

    ~qos_flow() {
        if (qos_handle_) {
            if (flow_id_) {
                if (!QOSRemoveSocketFromFlow(qos_handle_, 0, flow_id_, 0)) {
                    RAV_ERROR("Failed to close QOS flow");
                }
            }
            if (!QOSCloseHandle(qos_handle_)) {
                RAV_ERROR("Failed to close QOS handle");
            }
        }
    }

    qos_flow(const qos_flow&) = delete;
    qos_flow& operator=(const qos_flow&) = delete;

    qos_flow(qos_flow&&) noexcept = delete;
    qos_flow& operator=(qos_flow&&) noexcept = delete;

    /**
     * Adds given socket to the flow.
     * @param socket Socket to add.
     * @return True on success, or false on failure.
     */
    bool add_socket_to_flow(asio::ip::udp::socket& socket) {
        SOCKET native_socket_handle = socket.native_handle();

        asio::error_code ec;
        auto endpoint = socket.local_endpoint(ec);
        if (ec) {
            RAV_ERROR("Failed to get local endpoint");
            return false;
        }
        auto local_address = endpoint.address();
        if (!local_address.is_v4()) {
            RAV_ERROR("Socket must be ipv4");  // ipv6 not supported at the moment, but can be.
            return false;
        }

        SOCKADDR sockaddr;
        auto* sockaddr4 = reinterpret_cast<sockaddr_in*>(&sockaddr);
        sockaddr4->sin_family = AF_INET;
        sockaddr4->sin_port = htons(endpoint.port());
        sockaddr4->sin_addr.s_addr = htonl(local_address.to_v4().to_uint());

        if (!QOSAddSocketToFlow(
                qos_handle_, native_socket_handle, &sockaddr, QOSTrafficTypeBestEffort, QOS_NON_ADAPTIVE_FLOW, &flow_id_
            )) {
            RAV_ERROR("Failed to add socket to flow");
            return false;
        }

        return true;
    }

    /**
     * Sets the outgoing DSCP value on flow. At least one socket must have been added previously.
     * @param value The value to set.
     * @return True on success, of false on failure.
     */
    bool set_dscp_value(DWORD value) {
        if (qos_handle_ == nullptr) {
            RAV_ERROR("Invalid QOS handle");
            return false;
        }

        if (flow_id_ == 0) {
            RAV_ERROR("Invalid QOS flow id");
            return false;
        }

        if (!QOSSetFlow(qos_handle_, flow_id_, QOSSetOutgoingDSCPValue, sizeof(value), &value, 0, nullptr)) {
            RAV_ERROR("QOSSetFlow failed with error: {}", GetLastError());
            return false;
        }

        return true;
    }

    /**
     * @return True if a socket was added before, or false if no socket has been added.
     */
    [[nodiscard]] bool has_socket() const {
        return flow_id_ != 0;
    }

  private:
    QOS_VERSION version_ {1, 0};
    HANDLE qos_handle_ {};
    QOS_FLOWID flow_id_ {};
};

}  // namespace rav

#endif
