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
#include "ravennakit/core/expected.hpp"
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
class NetworkInterface {
  public:
    /// The identifier of a network interface (e.g. "en0", "eth0").
    using Identifier = std::string;

    /// The type of the network interface.
    enum class Type {
        undefined,
        wired_ethernet,
        wifi,
        cellular,
        loopback,
        other,
    };

    /// The capabilities of the network interface.
    struct Capabilities {
        bool hw_timestamp {false};
        bool sw_timestamp {false};
        bool multicast {false};

        [[nodiscard]] std::string to_string() const;

        friend bool operator==(const Capabilities& lhs, const Capabilities& rhs) {
            return lhs.hw_timestamp == rhs.hw_timestamp && lhs.sw_timestamp == rhs.sw_timestamp
                && lhs.multicast == rhs.multicast;
        }

        friend bool operator!=(const Capabilities& lhs, const Capabilities& rhs) {
            return !(lhs == rhs);
        }
    };

    /**
     * Constructs a network interface with the given identifier. The identifier is used to uniquely identify the
     * interface, and should be the BSD name on BSD-style platforms and the AdapterName on Windows platforms.
     * @param identifier The unique identifier of the network interface.
     */
    explicit NetworkInterface(Identifier identifier) : identifier_(std::move(identifier)) {
        RAV_ASSERT(!identifier_.empty(), "Identifier cannot be empty");
    }

    /**
     *
     * @return The name of the network interface.
     */
    [[nodiscard]] const Identifier& get_identifier() const;

    /**
     * @return The display name of the network interface.
     */
    [[nodiscard]] const std::string& get_display_name() const {
        return display_name_;
    }

    /**
     * @returns The display name of the network interface, including the identifier and the first ipv4 address.
     */
    [[nodiscard]] std::string get_extended_display_name() const {
        std::string display_name = display_name_;
        if (display_name_.empty())
            display_name = identifier_;
        for (const auto& addr : addresses_) {
            if (addr.is_v6() || addr.is_multicast() || addr.is_unspecified())
                continue;
            fmt::format_to(std::back_inserter(display_name), " ({}: {})", identifier_, addr.to_string());
            break;
        }
        return display_name;
    }

    /**
     * @return The description of the network interface.
     */
    [[nodiscard]] const std::string& get_description() const {
        return description_;
    }

    /**
     * @return The MAC address of the network interface, or nullopt if the interface does not have a MAC address.
     */
    [[nodiscard]] const std::optional<MacAddress>& get_mac_address() const {
        return mac_address_;
    }

    /**
     * @return The addresses of the interface.
     */
    [[nodiscard]] const std::vector<asio::ip::address>& get_addresses() const {
        return addresses_;
    }

    /**
     * @return The first IPv4 address of the interface, or nullopt if the interface does not have an IPv4 address.
     */
    asio::ip::address get_first_ipv4_address() const {
        for (const auto& addr : addresses_) {
            if (addr.is_v4()) {
                return addr;
            }
        }
        return {};
    }

    /**
     * @return The type of the interface.
     */
    [[nodiscard]] Type get_type() const {
        return type_;
    }

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
    [[nodiscard]] std::optional<uint32_t> get_interface_index() const;

    /**
     * @returns A description of the network interface as string.
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * Converts the given type to a string.
     * @param type The type to convert.
     * @returns The string representation of the type.
     */
    static const char* type_to_string(Type type);

    /**
     * @returns A list of all network interfaces on the system. Only several operating systems are supported: macOS,
     * Windows and Linux. Not Android.
     */
    static tl::expected<std::vector<NetworkInterface>, int> get_all();

    [[nodiscard]] auto tie() const {
        return std::tie(identifier_, display_name_, description_, mac_address_, addresses_, type_, capabilities_);
    }

    friend bool operator==(const NetworkInterface& lhs, const NetworkInterface& rhs) {
        return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(const NetworkInterface& lhs, const NetworkInterface& rhs) {
        return lhs.tie() != rhs.tie();
    }

  private:
    Identifier identifier_;
    std::string display_name_;
    std::string description_;
    std::optional<MacAddress> mac_address_;
    std::vector<asio::ip::address> addresses_;
    Type type_ {Type::undefined};
    Capabilities capabilities_ {};
#if RAV_WINDOWS
    IF_LUID if_luid_ {};
#endif
};

}  // namespace rav
