#include "ravennakit/core/log.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

#include <iostream>
#include <string>

int main(const int argc, char* argv[]) {
#if RAV_ENABLE_SPDLOG
    spdlog::set_level(spdlog::level::trace);
#endif

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

    browser->on<rav::dnssd::events::service_discovered>([](const rav::dnssd::events::service_discovered& event,
                                                           rav::dnssd::dnssd_browser&) {
        RAV_INFO("Service discovered: {}", event.description.description());
    });

    browser->on<rav::dnssd::events::service_removed>([](const rav::dnssd::events::service_removed& event,
                                                        rav::dnssd::dnssd_browser&) {
        RAV_INFO("Service removed: {}", event.description.description());
    });

    browser->on<rav::dnssd::events::service_resolved>([](const rav::dnssd::events::service_resolved& event,
                                                         rav::dnssd::dnssd_browser&) {
        RAV_INFO("Service resolved: {}", event.description.description());
    });

    browser->on<rav::dnssd::events::address_added>([](const rav::dnssd::events::address_added& event,
                                                      rav::dnssd::dnssd_browser&) {
        RAV_INFO("Address added ({}): {}", event.address, event.description.description());
    });

    browser->on<rav::dnssd::events::address_removed>([](const rav::dnssd::events::address_removed& event,
                                                        rav::dnssd::dnssd_browser&) {
        RAV_INFO("Address removed ({}): {}", event.address, event.description.description());
    });

    browser->on<rav::dnssd::events::browse_error>([](const rav::dnssd::events::browse_error& event,
                                                     rav::dnssd::dnssd_browser&) {
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
