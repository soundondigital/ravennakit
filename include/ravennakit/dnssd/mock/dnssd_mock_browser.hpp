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
#include "ravennakit/dnssd/dnssd_browser.hpp"

namespace rav::dnssd {

class MockBrowser: public Browser {
  public:
    explicit MockBrowser(boost::asio::io_context& io_context);
    ~MockBrowser() override = default;

    /**
     * Mocks discovering a service.
     * @param fullname The fullname of the service. Should not contain spaces.
     * @param name The name of the service.
     * @param reg_type The registration type of the service (i.e. _http._tcp.).
     * @param domain The domain of the service (i.e. local.).
     */
    void mock_discovered_service(
        const std::string& fullname, const std::string& name, const std::string& reg_type, const std::string& domain
    );

    /**
     * Mocks resolving a service. Requires calling mock_discovered_service before.
     * @param fullname The fullname of the service which was discovered before.
     * @param host_target The host target of the service.
     * @param port The port of the service.
     * @param txt_record The txt record of the service.
     */
    void mock_resolved_service(
        const std::string& fullname, const std::string& host_target, uint16_t port, const TxtRecord& txt_record
    );

    /**
     * Mocks adding an address to a service. Requires calling mock_discovered_service before.
     * @param fullname The fullname of the service which was discovered before.
     * @param address The address to add.
     * @param interface_index
     */
    void mock_added_address(const std::string& fullname, const std::string& address, uint32_t interface_index);

    /**
     * Mocks removing an address from a service. Requires calling mock_discovered_service before.
     * @param fullname The fullname of the service which was discovered before.
     * @param address The address to remove.
     * @param interface_index The interface index of the address.
     */
    void mock_removed_address(const std::string& fullname, const std::string& address, uint32_t interface_index);

    /**
     * Mocks removing a service. Requires calling mock_discovered_service before.
     * @param fullname The fullname of the service which was discovered before.
     */
    void mock_removed_service(const std::string& fullname);

    // dnssd_browser overrides
    void browse_for(const std::string& service_type) override;
    [[nodiscard]] const ServiceDescription* find_service(const std::string& service_name) const override;
    [[nodiscard]] std::vector<ServiceDescription> get_services() const override;

  private:
    boost::asio::io_context& io_context_;
    std::map<std::string, ServiceDescription> services_;  // fullname -> service description
    std::set<std::string> browsers_;                      // reg_type
};

}  // namespace rav::dnssd
