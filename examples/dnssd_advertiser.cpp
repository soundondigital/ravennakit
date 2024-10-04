#include "ravennakit/core/log.hpp"
#include "ravennakit/dnssd/service_description.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"

#include <iostream>
#include <string>
#include <vector>

static bool parse_txt_record(rav::dnssd::txt_record& txt_record, const std::string& string_value) {
    if (string_value.empty())
        return false;

    const size_t pos = string_value.find('=');
    if (pos != std::string::npos)
        txt_record[string_value.substr(0, pos)] = string_value.substr(pos + 1);
    else
        txt_record[string_value.substr(0, pos)] = "";

    return true;
}

int main(int const argc, char* argv[]) {
#if RAV_ENABLE_SPDLOG
    spdlog::set_level(spdlog::level::trace);
#endif

    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        args.emplace_back(argv[i]);
    }

    // Do we have at least the service type and port number?
    if (args.size() < 2) {
        std::cout << "Error: expected at least an argument which specifies the service type and an argument which "
                     "specifies the port number (example: _test._tcp 1234 key1=value1 key2=value2)"
                  << std::endl;
        return -1;
    }

    // Parse port number
    int port_number = 0;
    try {
        port_number = std::stoi(args[1]);
    } catch (const std::exception& e) {
        std::cout << "Invalid port number: " << e.what() << std::endl;
        return -1;
    }

    // Parse remaining arguments as TxtRecord
    rav::dnssd::txt_record txt_record;
    for (auto it = args.begin() + 2; it != args.end(); ++it) {
        parse_txt_record(txt_record, *it);
    }

    const auto advertiser = rav::dnssd::dnssd_advertiser::create();

    if (advertiser == nullptr) {
        RAV_ERROR("Error: no dnssd advertiser implementation available for this platform");
        return -1;
    }

    advertiser->on<rav::dnssd::events::advertiser_error>([](const rav::dnssd::events::advertiser_error& event,
                                                            rav::dnssd::dnssd_advertiser&) {
        RAV_ERROR("Advertiser error: {}", event.error_message);
    });

    advertiser->on<rav::dnssd::events::name_conflict>([](const rav::dnssd::events::name_conflict& event,
                                                         rav::dnssd::dnssd_advertiser&) {
        RAV_CRITICAL("Name conflict: {} {}", event.reg_type, event.name);
    });

    advertiser->register_service(
        args[0], "First test service", nullptr, static_cast<uint16_t>(port_number), txt_record, true
    );

    const auto service_id2 = advertiser->register_service(
        args[0], "Second test service", nullptr, static_cast<uint16_t>(port_number), txt_record, true
    );

    RAV_INFO("Enter key=value to update the TXT record, or q to exit...");

    std::string cmd;
    while (true) {
        std::getline(std::cin, cmd);
        if (cmd == "q" || cmd == "Q") {
            break;
        }

        if (cmd == "r" || cmd == "R") {
            advertiser->unregister_service(service_id2);
            continue;
        }

        try {
            if (parse_txt_record(txt_record, cmd)) {
                advertiser->update_txt_record(service_id2, txt_record);

                RAV_INFO("Updated txt record:");

                for (auto& pair : txt_record) {
                    RAV_INFO("{}={}", pair.first, pair.second);
                }
            }
        } catch (const std::exception& e) {
            RAV_ERROR("Failed to update txt record: {}", e.what());
        }
    }

    std::cout << "Exit" << std::endl;

    return 0;
}
