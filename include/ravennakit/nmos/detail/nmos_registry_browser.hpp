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

#include "nmos_api_version.hpp"
#include "nmos_discover_mode.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/net/timer/asio_timer.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

namespace rav::nmos {

class RegistryBrowser {
  public:
    SafeFunction<void(const dnssd::ServiceDescription&)> on_registry_discovered;

    using Callback = std::function<void(std::optional<dnssd::ServiceDescription>)>;

    explicit RegistryBrowser(boost::asio::io_context& io_context) : io_context_(io_context) {}

    void start(const DiscoverMode discover_mode, const ApiVersion api_version) {
        discover_mode_ = discover_mode;
        api_version_ = api_version;

        if (discover_mode == DiscoverMode::manual) {
            multicast_browser_.reset();
            unicast_browser_.reset();
            return;  // No discovery needed in manual mode
        }

        // Unicast
        if (discover_mode_ == DiscoverMode::dns || discover_mode_ == DiscoverMode::udns) {
            unicast_browser_ = dnssd::Browser::create(io_context_);
            // TODO: Start a unicast query
        } else {
            unicast_browser_.reset();
        }

        // Multicast
        if (discover_mode_ == DiscoverMode::dns || discover_mode_ == DiscoverMode::mdns) {
            if (multicast_browser_ == nullptr) {
                multicast_browser_ = dnssd::Browser::create(io_context_);
                multicast_browser_->on_service_resolved = [this](const dnssd::ServiceDescription& desc) {
                    if (filter_and_get_pri(desc).has_value()) {
                        on_registry_discovered(desc);
                    } else {
                        RAV_TRACE("Service {} does not match NMOS registry criteria, ignoring", desc.name);
                    }
                };
                multicast_browser_->browse_for("_nmos-register._tcp");
                multicast_browser_->browse_for("_nmos-registration._tcp");  // For v1.2 compatibility
            }
        } else {
            multicast_browser_.reset();
        }
    }

    void stop() {
        unicast_browser_.reset();
        multicast_browser_.reset();
    }

    [[nodiscard]] std::vector<dnssd::ServiceDescription> get_services() const {
        std::vector<dnssd::ServiceDescription> services;
        if (unicast_browser_) {
            auto unicast_services = unicast_browser_->get_services();
            services.insert(services.end(), unicast_services.begin(), unicast_services.end());
        }
        if (multicast_browser_) {
            auto multicast_services = multicast_browser_->get_services();
            services.insert(services.end(), multicast_services.begin(), multicast_services.end());
        }
        return services;
    }

    [[nodiscard]] std::optional<dnssd::ServiceDescription> find_most_suitable_registry() const {
        std::optional<dnssd::ServiceDescription> best_desc;
        int best_pri = std::numeric_limits<int>::max();

        // Unicast DNS
        if (unicast_browser_ != nullptr) {
            for (auto& desc : unicast_browser_->get_services()) {
                auto pri = filter_and_get_pri(desc);
                if (!pri) {
                    continue;
                }
                if (*pri > best_pri) {
                    continue;
                }
                best_pri = *pri;
                best_desc = desc;
            }
            if (best_desc) {
                return *best_desc;
            }
        }

        // Multicast DNS
        if (multicast_browser_ != nullptr) {
            for (auto& desc : multicast_browser_->get_services()) {
                auto pri = filter_and_get_pri(desc);
                if (!pri) {
                    continue;
                }
                if (*pri < best_pri) {
                    best_pri = *pri;
                    best_desc = desc;
                }
            }
        }

        return best_desc;
    }

  private:
    boost::asio::io_context& io_context_;
    DiscoverMode discover_mode_ {};
    ApiVersion api_version_;
    std::unique_ptr<dnssd::Browser> unicast_browser_;
    std::unique_ptr<dnssd::Browser> multicast_browser_;

    [[nodiscard]] std::optional<int> filter_and_get_pri(const dnssd::ServiceDescription& desc) const {
        if (desc.reg_type != "_nmos-register._tcp." && desc.reg_type != "_nmos-registration._tcp.") {
            return std::nullopt;
        }

        // api_proto
        const auto api_proto = desc.txt.find("api_proto");
        if (api_proto == desc.txt.end()) {
            return std::nullopt;
        }
        if (api_proto->second != "http") {
            return std::nullopt;  // TODO: Only http supported
        }

        // api_ver
        const auto api_ver = desc.txt.find("api_ver");
        if (api_ver == desc.txt.end()) {
            return std::nullopt;
        }
        if (!string_contains(api_ver->second, api_version_.to_string())) {
            return std::nullopt;
        }

        // api_auth
        const auto api_auth = desc.txt.find("api_auth");
        if (api_auth == desc.txt.end()) {
            return std::nullopt;
        }
        if (api_auth->second != "false") {
            return std::nullopt;  // TODO: Only no auth supported
        }

        // pri
        const auto pri = desc.txt.find("pri");
        if (pri == desc.txt.end()) {
            return std::nullopt;
        }
        return string_to_int<int>(pri->second, true);
    }
};

}  // namespace rav::nmos
