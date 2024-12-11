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

#include "mac_address.hpp"
#include "ravennakit/core/platform.hpp"
#include <vector>
#include <asio/ip/address.hpp>
#include <tl/expected.hpp>
#include <optional>

#if RAV_WINDOWS
    #include <ifdef.h>
#endif

#if RAV_WINDOWS
    #define HAS_WIN32 1
#else
    #define HAS_WIN32 0
#endif

#if RAV_ANDROID
    #define HAS_BSD_SOCKETS 0
#elif RAV_POSIX
    #define HAS_BSD_SOCKETS 1
#else
    #define HAS_BSD_SOCKETS 0
#endif

namespace rav {

/**
 * Represents a network interface in the system.
 */
class network_interface {
  public:
    /// The type of the network interface.
    enum class type {
        undefined,
        wired_ethernet,
        wifi,
        cellular,
        loopback,
        other,
    };

    /// The capabilities of the network interface.
    struct capabilities {
        bool hw_timestamp {false};
        bool sw_timestamp {false};
        bool multicast {false};

        [[nodiscard]] std::string to_string() const;
    };

    /**
     * Constructs a network interface with the given identifier. The identifier is used to uniquely identify the
     * interface, and should be the BSD name on BSD-style platforms and the AdapterName on Windows platforms.
     * @param identifier The unique identifier of the network interface.
     */
    explicit network_interface(std::string identifier) : identifier_(std::move(identifier)) {}

    /**
     *
     * @return The name of the network interface.
     */
    [[nodiscard]] const std::string& identifier() const;

#if HAS_WIN32 || defined(GENERATING_DOCUMENTATION)
    /**
     * @return The LUID of the interface.
     */
    [[maybe_unused]] IF_LUID get_interface_luid();
#endif

    /**
     * @returns The index of the network interface, or nullopt if the index could not be found.
     * Note: this is the index as defined by the operating system.
     */
    [[nodiscard]] std::optional<uint32_t> interface_index() const;

    /**
     * @returns A description of the network interface as string.
     */
    std::string to_string();

    /**
     * Converts the given type to a string.
     * @param type The type to convert.
     * @returns The string representation of the type.
     */
    static const char* type_to_string(type type);

    /**
     *
     * @returns A list of all network interfaces on the system. Only several operating systems are supported: macOS,
     * Windows and Linux. Not Android.
     */
    static tl::expected<std::vector<network_interface>, int> get_all();

  private:
    std::string identifier_;
    std::string display_name_;
    std::string description_;
    std::optional<mac_address> mac_address_;
    std::vector<asio::ip::address> addresses_;
    type type_ {type::undefined};
    capabilities capabilities_ {};
#if RAV_WINDOWS
    IF_LUID if_luid_ {};
#endif

    void determine_capabilities();
};

}  // namespace rav
