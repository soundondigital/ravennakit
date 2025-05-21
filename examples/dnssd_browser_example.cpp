#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

#include <iostream>
#include <string>

int main(const int argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    if (argc < 2) {
        std::cout << "Expected an argument which specifies the service type to browse for (example: _http._tcp)"
                  << std::endl;
        return -1;
    }

    boost::asio::io_context io_context;  // NOLINT

    const auto browser = rav::dnssd::Browser::create(io_context);

    if (browser == nullptr) {
        std::cout << "No browser implementation available for this platform" << std::endl;
        exit(-1);
    }

    browser->on<rav::dnssd::Browser::ServiceDiscovered>([](const auto& event) {
        RAV_INFO("Service discovered: {}", event.description.to_string());
    });

    browser->on<rav::dnssd::Browser::ServiceRemoved>([](const auto& event) {
        RAV_INFO("Service removed: {}", event.description.to_string());
    });

    browser->on<rav::dnssd::Browser::ServiceResolved>([](const auto& event) {
        RAV_INFO("Service resolved: {}", event.description.to_string());
    });

    browser->on<rav::dnssd::Browser::AddressAdded>([](const auto& event) {
        RAV_INFO("Address added ({}): {}", event.address, event.description.to_string());
    });

    browser->on<rav::dnssd::Browser::AddressRemoved>([](const auto& event) {
        RAV_INFO("Address removed ({}): {}", event.address, event.description.to_string());
    });

    browser->on<rav::dnssd::Browser::BrowseError>([](const auto& event) {
        RAV_ERROR("{}", event.error_message);
    });

    browser->browse_for(argv[1]);

    std::thread io_context_thread([&io_context] {
        io_context.run();
    });

    std::cout << "Press enter to exit..." << std::endl;
    std::cin.get();

    io_context.stop();
    io_context_thread.join();

    std::cout << "Exit" << std::endl;

    return 0;
}
