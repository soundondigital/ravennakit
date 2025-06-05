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
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/net/timer/asio_timer.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

namespace rav::nmos {

class RegistryBrowserBase {
  public:
    SafeFunction<void(const dnssd::ServiceDescription&)> on_registry_discovered;

    virtual ~RegistryBrowserBase() = default;
    virtual void start(OperationMode operation_mode, ApiVersion api_version) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual std::optional<dnssd::ServiceDescription> find_most_suitable_registry() const = 0;

    [[nodiscard]] static std::optional<int>
    filter_and_get_pri(const dnssd::ServiceDescription& desc, const ApiVersion api_version) {
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
        if (!string_contains(api_ver->second, api_version.to_string())) {
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

class RegistryBrowser final: public RegistryBrowserBase {
  public:
    using BrowserFactory = std::function<std::unique_ptr<dnssd::Browser>(boost::asio::io_context&)>;

    explicit RegistryBrowser(
        boost::asio::io_context& io_context, BrowserFactory unicast_browser_factory = nullptr,
        BrowserFactory multicast_browser_factory = nullptr
    ) :
        io_context_(io_context),
        unicast_browser_factory_(std::move(unicast_browser_factory)),
        multicast_browser_factory_(std::move(multicast_browser_factory)) {}

    void start(OperationMode operation_mode, const ApiVersion api_version) override {
        operation_mode_ = operation_mode;
        api_version_ = api_version;

        if (operation_mode == OperationMode::manual) {
            multicast_browser_.reset();
            return;  // No discovery needed in manual mode
        }

        // Multicast
        if (operation_mode_ == OperationMode::mdns_p2p) {
            if (multicast_browser_ == nullptr) {
                multicast_browser_ = multicast_browser_factory_ ? multicast_browser_factory_(io_context_)
                                                                : dnssd::Browser::create(io_context_);
                multicast_browser_->on_service_resolved = [this](const dnssd::ServiceDescription& desc) {
                    if (filter_and_get_pri(desc, api_version_).has_value()) {
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

    void stop() override {
        multicast_browser_.reset();
    }

    [[nodiscard]] std::vector<dnssd::ServiceDescription> get_services() const {
        if (multicast_browser_) {
            return multicast_browser_->get_services();
        }
        return {};
    }

    [[nodiscard]] std::optional<dnssd::ServiceDescription> find_most_suitable_registry() const override {
        std::optional<dnssd::ServiceDescription> best_desc;
        int best_pri = std::numeric_limits<int>::max();

        // Multicast DNS
        if (multicast_browser_ != nullptr) {
            for (auto& desc : multicast_browser_->get_services()) {
                auto pri = filter_and_get_pri(desc, api_version_);
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
    BrowserFactory unicast_browser_factory_;
    BrowserFactory multicast_browser_factory_;
    OperationMode operation_mode_ {};
    ApiVersion api_version_;
    std::unique_ptr<dnssd::Browser> multicast_browser_;
};

}  // namespace rav::nmos
