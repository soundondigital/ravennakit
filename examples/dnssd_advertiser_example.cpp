#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/dnssd/dnssd_service_description.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <asio/post.hpp>
#include <thread>

namespace examples {

static bool parse_txt_record(rav::dnssd::TxtRecord& txt_record, const std::string& string_value) {
    if (string_value.empty())
        return false;

    const size_t pos = string_value.find('=');
    if (pos != std::string::npos)
        txt_record[string_value.substr(0, pos)] = string_value.substr(pos + 1);
    else
        txt_record[string_value.substr(0, pos)] = "";

    return true;
}

}  // namespace examples

int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

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
    rav::dnssd::TxtRecord txt_record;
    for (auto it = args.begin() + 2; it != args.end(); ++it) {
        examples::parse_txt_record(txt_record, *it);
    }

    asio::io_context io_context;

    const auto advertiser = rav::dnssd::Advertiser::create(io_context);

    if (advertiser == nullptr) {
        RAV_ERROR("Error: no dnssd advertiser implementation available for this platform");
        return -1;
    }

    rav::dnssd::Advertiser::Subscriber subscriber;

    subscriber->on<rav::dnssd::Advertiser::AdvertiserError>([](const auto& event) {
        RAV_ERROR("Advertiser error: {}", event.error_message);
    });

    subscriber->on<rav::dnssd::Advertiser::NameConflict>([](const auto& event) {
        RAV_CRITICAL("Name conflict: {} {}", event.reg_type, event.name);
    });

    advertiser->subscribe(subscriber);
    advertiser->register_service(
        args[0], "Test service", nullptr, static_cast<uint16_t>(port_number), txt_record, true, false
    );

    const auto service_id2 = advertiser->register_service(
        args[0], "Test service", nullptr, static_cast<uint16_t>(port_number + 1), txt_record, true, false
    );

    std::thread io_context_thread([&io_context] {
        io_context.run();
    });

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
            if (examples::parse_txt_record(txt_record, cmd)) {
                // Schedule the updates on the io_context thread because the advertiser is not thread-safe.
                asio::post(io_context, [=, &advertiser] {
                    advertiser->update_txt_record(service_id2, txt_record);
                    RAV_INFO("Updated txt record");
                });
            }
        } catch (const std::exception& e) {
            RAV_ERROR("Failed to update txt record: {}", e.what());
        }
    }

    io_context.stop();
    io_context_thread.join();

    std::cout << "Exit" << std::endl;

    return 0;
}
