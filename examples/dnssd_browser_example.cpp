#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

#include <iostream>
#include <string>

int main(const int argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    if (argc < 2) {
        std::cout << "Expected an argument which specifies the service type to browse for (example: _http._tcp)"
                  << std::endl;
        return -1;
    }

    asio::io_context io_context;  // NOLINT

    const auto browser = rav::dnssd::dnssd_browser::create(io_context);

    if (browser == nullptr) {
        std::cout << "No browser implementation available for this platform" << std::endl;
        exit(-1);
    }

    rav::dnssd::dnssd_browser::subscriber subscriber;

    subscriber->on<rav::dnssd::dnssd_browser::service_discovered>([](const auto& event) {
        RAV_INFO("Service discovered: {}", event.description.description());
    });

    subscriber->on<rav::dnssd::dnssd_browser::service_removed>([](const auto& event) {
        RAV_INFO("Service removed: {}", event.description.description());
    });

    subscriber->on<rav::dnssd::dnssd_browser::service_resolved>([](const auto& event) {
        RAV_INFO("Service resolved: {}", event.description.description());
    });

    subscriber->on<rav::dnssd::dnssd_browser::address_added>([](const auto& event) {
        RAV_INFO("Address added ({}): {}", event.address, event.description.description());
    });

    subscriber->on<rav::dnssd::dnssd_browser::address_removed>([](const auto& event) {
        RAV_INFO("Address removed ({}): {}", event.address, event.description.description());
    });

    subscriber->on<rav::dnssd::dnssd_browser::browse_error>([](const auto& event) {
        RAV_ERROR("{}", event.error_message);
    });

    browser->subscribe(subscriber);
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
