/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/interfaces/network_interface.hpp"
#include "ravennakit/core/platform.hpp"
#include "ravennakit/core/subscription.hpp"
#include "ravennakit/core/net/interfaces/mac_address.hpp"
#include "ravennakit/core/platform/windows/wide_string.hpp"

#include <fmt/xchar.h>

#if RAV_POSIX
    #include <ifaddrs.h>
    #include <iomanip>
    #include <arpa/inet.h>
    #include <asio/ip/address.hpp>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #if RAV_APPLE
        #include <net/if_dl.h>
        #include "ravennakit/core/platform/apple/core_foundation/cf_array.hpp"
        #include "ravennakit/core/platform/apple/core_foundation/cf_string.hpp"
        #include "ravennakit/core/platform/apple/core_foundation/cf_type.hpp"
        #include "ravennakit/core/platform/apple/system_configuration/sc_network_interface.hpp"
        #include "ravennakit/core/platform/apple/system_configuration/sc_network_service.hpp"
        #include "ravennakit/core/platform/apple/system_configuration/sc_preferences.hpp"
    #elif RAV_LINUX
        #include <linux/if_packet.h>
    #endif
#elif RAV_WINDOWS
    #include <Iphlpapi.h>
    #include <ws2tcpip.h>

    #pragma comment(lib, "Iphlpapi")
    #pragma comment(lib, "Ws2_32")
#endif

#if RAV_APPLE
namespace {

// Get the type of the interface
rav::network_interface::type functional_type_for_interface(const char* name) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return rav::network_interface::type::undefined;
    }

    ifreq ifr = {};
    strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

    const bool success = ioctl(fd, SIOCGIFFUNCTIONALTYPE, &ifr) >= 0;

    const int junk = close(fd);
    assert(junk == 0);

    if (!success) {
        return rav::network_interface::type::undefined;
    }

    switch (ifr.ifr_ifru.ifru_functional_type) {
        case IFRTYPE_FUNCTIONAL_LOOPBACK:
            return rav::network_interface::type::loopback;
        case IFRTYPE_FUNCTIONAL_WIRED:
            return rav::network_interface::type::wired_ethernet;
        case IFRTYPE_FUNCTIONAL_WIFI_INFRA:
        case IFRTYPE_FUNCTIONAL_WIFI_AWDL:
            return rav::network_interface::type::wifi;
        case IFRTYPE_FUNCTIONAL_CELLULAR:
            return rav::network_interface::type::cellular;
        case IFRTYPE_FUNCTIONAL_UNKNOWN:
        default:
            return rav::network_interface::type::other;
    }
}

}  // namespace
#endif

std::optional<uint32_t> rav::network_interface::interface_index() const {
#if HAS_BSD_SOCKETS
    auto index = if_nametoindex(identifier_.c_str());
    if (index == 0) {
        return std::nullopt;
    }
    return index;
#elif HAS_WIN32
    NET_IFINDEX index {};
    auto result = ConvertInterfaceLuidToIndex(&if_luid_, &index);
    if (result != NO_ERROR) {
        RAV_ERROR("Failed to get interface index");
        return std::nullopt;
    }
    return index;
#else
    return std::nullopt;
#endif
}

std::string rav::network_interface::to_string() {
    std::string output = fmt::format("{}\n", identifier_);

    if (!display_name_.empty()) {
        fmt::format_to(std::back_inserter(output), "  display_name:\n    {}\n", display_name_);
    }

    if (!description_.empty()) {
        fmt::format_to(std::back_inserter(output), "  description:\n    {}\n", description_);
    }

    if (mac_address_.has_value()) {
        fmt::format_to(std::back_inserter(output), "  mac:\n    {}\n", mac_address_->to_string());
    }

    auto caps = capabilities_.to_string();
    if (!caps.empty()) {
        fmt::format_to(std::back_inserter(output), "  capabilities:\n   {}\n", caps);
    }

    fmt::format_to(std::back_inserter(output), "  type:\n    {}\n", type_to_string(type_));

    fmt::format_to(std::back_inserter(output), "  index:\n    {}\n", interface_index().value_or(0));

    if (!addresses_.empty()) {
        fmt::format_to(std::back_inserter(output), "  addrs:\n");
        for (const auto& address : addresses_) {
            fmt::format_to(std::back_inserter(output), "    {}\n", address.to_string());
        }
    }

    return output;
}

const char* rav::network_interface::type_to_string(const type type) {
    switch (type) {
        case type::wired_ethernet:
            return "wired_ethernet";
        case type::wifi:
            return "wifi";
        case type::cellular:
            return "cellular";
        case type::loopback:
            return "loopback";
        case type::other:
            return "other";
        case type::undefined:
        default:
            return "undefined";
    }
}

#if HAS_WIN32
[[maybe_unused]] IF_LUID rav::network_interface::get_interface_luid() {
    return if_luid_;
}
#endif

tl::expected<std::vector<rav::network_interface>, int> rav::network_interface::get_all() {
    std::vector<network_interface> network_interfaces;

#if HAS_BSD_SOCKETS
    ifaddrs* ifap = nullptr;

    // Get the list of network interfaces
    if (getifaddrs(&ifap) != 0) {
        return tl::unexpected(errno);
    }

    defer cleanup([&ifap] {
        freeifaddrs(ifap);
    });

    for (const ifaddrs* ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_name == nullptr) {
            RAV_WARNING("Network interface name is null");
            continue;
        }

        auto it = std::find_if(
            network_interfaces.begin(), network_interfaces.end(),
            [&ifa](const network_interface& network_interface) {
                return network_interface.identifier_ == ifa->ifa_name;
            }
        );

        if (it == network_interfaces.end()) {
            it = network_interfaces.emplace(it, ifa->ifa_name);
        }

        // Address
        if (ifa->ifa_addr) {
            if (ifa->ifa_addr->sa_family == AF_INET) {
                const sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                asio::ip::address_v4 addr_v4(ntohl(sa->sin_addr.s_addr));
                it->addresses_.emplace_back(addr_v4);
            } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                const sockaddr_in6* sa = reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
                asio::ip::address_v6::bytes_type bytes;
                std::memcpy(bytes.data(), &sa->sin6_addr, sizeof(bytes));
                asio::ip::address_v6 addr_v6(bytes, sa->sin6_scope_id);
                it->addresses_.emplace_back(addr_v6);
    #if RAV_APPLE
            } else if (ifa->ifa_addr->sa_family == AF_LINK) {
                const sockaddr_dl* sdl = reinterpret_cast<struct sockaddr_dl*>(ifa->ifa_addr);
                if (sdl->sdl_alen == 6) {  // MAC addresses are 6 bytes
                    it->mac_address_ = mac_address(reinterpret_cast<uint8_t*>(LLADDR(sdl)));
                }
    #elif RAV_LINUX
            } else if (ifa->ifa_addr->sa_family == AF_PACKET) {
                const sockaddr_ll* sdl = reinterpret_cast<struct sockaddr_ll*>(ifa->ifa_addr);
                it->set_mac_address(mac_address(sdl->sll_addr));
    #endif
            }
        }

        it->determine_capabilities();

    #if RAV_APPLE
        it->type_ = functional_type_for_interface(ifa->ifa_name);
    #endif
    }

    #if RAV_APPLE
    // Fill in the network display name

    const auto interfaces = sc_preferences::get_network_interfaces();

    if (!interfaces) {
        RAV_ERROR("Failed to get network interfaces");
        return tl::unexpected(-1);
    }

    for (CFIndex i = 0; i < interfaces.count(); ++i) {
        const auto interface = sc_network_interface(interfaces[i], true);
        if (!interface) {
            continue;
        }

        auto bsd_name = interface.get_bsd_name();
        if (bsd_name.empty()) {
            continue;
        }

        auto it = std::find_if(
            network_interfaces.begin(), network_interfaces.end(),
            [&bsd_name](const network_interface& network_interface) {
                return network_interface.identifier_ == bsd_name;
            }
        );

        if (it == network_interfaces.end()) {
            continue;  // We're only filling in the existing interfaces, skipping interfaces not gotten from getifaddrs.
        }

        it->display_name_ = interface.get_localized_display_name();
    }
    #endif

#elif HAS_WIN32

    ULONG bufferSize = 15'000;  // As per recommendation from Microsoft
    std::vector<uint8_t> buffer(bufferSize);
    auto* addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    auto result = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addresses, &bufferSize);

    if (result != NO_ERROR) {
        if (result == ERROR_BUFFER_OVERFLOW) {
            // The buffer was not big enough, do a 2nd attempt.
            buffer.resize(bufferSize);
            addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
            result = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addresses, &bufferSize);
            if (result != NO_ERROR) {
                RAV_ERROR("Failed to get network interface information ({})", result);
                return {};
            }
        } else {
            RAV_ERROR("Failed to get network interface information ({})", result);
            return {};
        }
    }

    for (IP_ADAPTER_ADDRESSES* adapter = addresses; adapter != nullptr; adapter = adapter->Next) {
        const auto* adapter_name = adapter->AdapterName;
        if (adapter_name == nullptr) {
            continue;
        }

        auto it = std::find_if(
            network_interfaces.begin(), network_interfaces.end(),
            [&adapter_name](const network_interface& network_interface) {
                return network_interface.identifier_ == adapter_name;
            }
        );

        if (it == network_interfaces.end()) {
            it = network_interfaces.emplace(it, adapter_name);
        }

        it->display_name_ = wide_string_to_string(adapter->FriendlyName);
        it->description_ = wide_string_to_string(adapter->Description);
        it->if_luid_ = adapter->Luid;

        if (adapter->PhysicalAddressLength == 6) {
            it->mac_address_ = mac_address(adapter->PhysicalAddress);
        } else if (adapter->PhysicalAddressLength > 0) {
            RAV_WARNING("Unknown physical address length ({})", adapter->PhysicalAddressLength);
        }

        for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != nullptr;
             unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                const sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(unicast->Address.lpSockaddr);
                asio::ip::address_v4 addr_v4(ntohl(sa->sin_addr.s_addr));
                it->addresses_.emplace_back(addr_v4);
            } else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) {
                const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(unicast->Address.lpSockaddr);
                asio::ip::address_v6::bytes_type bytes;
                std::memcpy(bytes.data(), &ipv6->sin6_addr, bytes.size());
                it->addresses_.emplace_back(asio::ip::address_v6(bytes, adapter->Ipv6IfIndex));
            }
        }

        switch (adapter->IfType) {
            case IF_TYPE_ETHERNET_CSMACD:
                it->type_ = type::wired_ethernet;
                break;
            case IF_TYPE_SOFTWARE_LOOPBACK:
                it->type_ = type::loopback;
                break;
            case IF_TYPE_IEEE80211:
                it->type_ = type::wifi;
                break;
            default:
                it->type_ = type::other;
        }
    }
#endif

    return network_interfaces;
}

void rav::network_interface::determine_capabilities() {
#if HAS_BSD_SOCKETS
    ifreq ifr {};

    const auto fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        RAV_ERROR("Failed to open socket");
        return;
    }

    defer close_socket([&] {
        close(fd);
    });

    std::strncpy(ifr.ifr_name, identifier_.c_str(), identifier_.size());

    // Query the interface capabilities
    if (ioctl(fd, SIOCGIFCAP, &ifr) < 0) {
        RAV_ERROR("Failed to query interface capabilities");
        return;
    }

    capabilities caps;
    caps.hw_timestamp = ifr.ifr_curcap & IFCAP_HW_TIMESTAMP;
    caps.sw_timestamp = ifr.ifr_curcap & IFCAP_SW_TIMESTAMP;

    // Query the interface flags
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        RAV_ERROR("Failed to query interface flags");
        return;
    }

    if (ifr.ifr_flags & IFF_MULTICAST) {
        caps.multicast = true;
    }

    capabilities_ = caps;
#endif
}

std::string rav::network_interface::capabilities::to_string() const {
    std::string output;
    if (hw_timestamp) {
        fmt::format_to(std::back_inserter(output), " HW_TIMESTAMP");
    }
    if (sw_timestamp) {
        fmt::format_to(std::back_inserter(output), " SW_TIMESTAMP");
    }
    if (multicast) {
        fmt::format_to(std::back_inserter(output), " MULTICAST");
    }
    return output;
}

const std::string& rav::network_interface::identifier() const {
    return identifier_;
}
