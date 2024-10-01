#include "ravennakit/core/log.hpp"
#include "ravennakit/dnssd/service_description.hpp"
#include "ravennakit/dnssd/bonjour/bonjour_advertiser.hpp"

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

    rav::dnssd::bonjour_advertiser advertiser;

    advertiser.on<rav::dnssd::events::advertiser_error>([](const rav::dnssd::events::advertiser_error& event,
                                                           rav::dnssd::dnssd_advertiser&) {
        RAV_CRITICAL("Exception caught: {}", event.exception.what());
    });

    advertiser.register_service(
        args[0], "001122334455@SomeName", nullptr, static_cast<uint16_t>(port_number), txt_record
    );

    std::cout << "Enter key=value to update the TXT record, or q to exit..." << std::endl;

    std::string cmd;
    while (true) {
        std::getline(std::cin, cmd);
        if (cmd == "q" || cmd == "Q") {
            break;
        }

        if (parse_txt_record(txt_record, cmd)) {
            std::cout << "Updated txt record: " << std::endl;
            for (auto& pair : txt_record) {
                std::cout << pair.first << "=" << pair.second << std::endl;
            }

            advertiser.update_txt_record(txt_record);
        }
    }

    std::cout << "Exit" << std::endl;

    return 0;
}
