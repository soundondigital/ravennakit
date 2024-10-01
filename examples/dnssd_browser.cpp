#include "ravennakit/core/log.hpp"
#include "ravennakit/dnssd/bonjour/bonjour_browser.hpp"

#include <iostream>
#include <string>

int main(const int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Expected an argument which specifies the service type to browse for (example: _http._tcp)"
                  << std::endl;
        return -1;
    }

    const auto browser = rav::dnssd::dnssd_browser::create();

    if (browser == nullptr) {
        std::cout << "No browser implementation available for this platform" << std::endl;
        exit(-1);
    }

    browser->on<rav::dnssd::events::service_discovered>([](const rav::dnssd::events::service_discovered& event,
                                                           rav::dnssd::dnssd_browser&) {
        RAV_INFO("Service discovered: {}", event.service_description.description());
    });

    browser->on<rav::dnssd::events::service_removed>([](const rav::dnssd::events::service_removed& event,
                                                        rav::dnssd::dnssd_browser&) {
        RAV_INFO("Service removed: {}", event.service_description.description());
    });

    browser->on<rav::dnssd::events::service_resolved>([](const rav::dnssd::events::service_resolved& event,
                                                         rav::dnssd::dnssd_browser&) {
        RAV_INFO("Service resolved: {}", event.service_description.description());
    });

    browser->on<rav::dnssd::events::address_added>([](const rav::dnssd::events::address_added& event,
                                                      rav::dnssd::dnssd_browser&) {
        RAV_INFO("Address added ({}): {}", event.address, event.service_description.description());
    });

    browser->on<rav::dnssd::events::address_removed>([](const rav::dnssd::events::address_removed& event,
                                                        rav::dnssd::dnssd_browser&) {
        RAV_INFO("Address removed ({}): {}", event.address, event.service_description.description());
    });

    browser->on<rav::dnssd::events::browse_error>([](const rav::dnssd::events::browse_error& event,
                                                     rav::dnssd::dnssd_browser&) {
        RAV_INFO("Error: {}", event.error.description());
    });

    auto const result = browser->browse_for(argv[1]);
    if (result.has_error()) {
        std::cout << "Error: " << result.description() << std::endl;
        return -1;
    };

    std::cout << "Press enter to exit..." << std::endl;

    std::string cmd;
    std::getline(std::cin, cmd);

    std::cout << "Exit" << std::endl;

    return 0;
}
