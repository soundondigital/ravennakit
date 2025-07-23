/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_node.hpp"

#include "ravennakit/core/platform/apple/priority.hpp"
#include "ravennakit/core/platform/windows/thread_characteristics.hpp"
#include "ravennakit/ravenna/ravenna_sender.hpp"

#include <utility>

rav::RavennaNode::RavennaNode() :
    rtsp_server_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), 0)),
    ptp_instance_(io_context_) {
    advertiser_ = dnssd::Advertiser::create(io_context_);

    nmos_device_.id = boost::uuids::random_generator()();
    if (!nmos_node_.add_or_update_device(nmos_device_)) {
        RAV_ERROR("Failed to add NMOS device with ID: {}", boost::uuids::to_string(nmos_device_.id));
    }

    nmos_node_.on_configuration_changed = [this](const nmos::Node::Configuration& config) {
        for (const auto& s : subscribers_) {
            s->nmos_node_config_updated(config);
        }
    };

    nmos_node_.on_status_changed =
        [this](const nmos::Node::Status status, const nmos::Node::StatusInfo& registry_info) {
            for (const auto& s : subscribers_) {
                s->nmos_node_status_changed(status, registry_info);
            }
        };

    std::promise<std::thread::id> promise;
    auto f = promise.get_future();
    maintenance_thread_ = std::thread([this, p = std::move(promise)]() mutable {
        p.set_value(std::this_thread::get_id());
        TRACY_SET_THREAD_NAME("ravenna_node_maintenance");
#if RAV_APPLE
        pthread_setname_np("ravenna_node_maintenance");
#endif

        while (true) {
            try {
                while (!io_context_.stopped()) {
                    io_context_.run_for(std::chrono::seconds(1));
                    do_maintenance();
                }
                break;
            } catch (const std::exception& e) {
                RAV_ERROR("Unhandled exception on maintenance thread: {}", e.what());
                RAV_ASSERT_FALSE("Unhandled exception on maintenance thread");
            }
        }
    });
    maintenance_thread_id_ = f.get();

    network_thread_ = std::thread([this] {
        TRACY_SET_THREAD_NAME("ravenna_node_network");
#if RAV_APPLE
        pthread_setname_np("ravenna_node_network");
        constexpr auto min_packet_time = 125 * 1000;       // 125us
        constexpr auto max_packet_time = 4 * 1000 * 1000;  // 4ms
        if (!set_thread_realtime(min_packet_time, max_packet_time, max_packet_time * 2)) {
            RAV_ERROR("Failed to set thread realtime");
        }
#endif

#if RAV_WINDOWS
        WindowsThreadCharacteristics set_thread_characteristics(TEXT("Pro Audio"));
#endif

        while (keep_going_.load(std::memory_order_acquire)) {
            try {
                while (keep_going_.load(std::memory_order_acquire)) {
                    rtp_receiver_.read_incoming_packets();
                    rtp_sender_.send_outgoing_packets();
#if !RAV_WINDOWS
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
#endif
                }
                break;
            } catch (const std::exception& e) {
                RAV_ERROR("Unhandled exception on network thread: {}", e.what());
                RAV_ASSERT_FALSE("Unhandled exception on network thread");
            }
        }
    });
}

rav::RavennaNode::~RavennaNode() {
    io_context_.stop();
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
    keep_going_.store(false, std::memory_order_release);
    if (network_thread_.joinable()) {
        network_thread_.join();
    }
    for (const auto& receiver : receivers_) {
        receiver->set_nmos_node(nullptr);  // Prevent receiver from sending NMOS updates upon destruction
    }
    for (const auto& sender : senders_) {
        sender->set_nmos_node(nullptr);  // Prevent receiver from sending NMOS updates upon destruction
    }
}

std::future<tl::expected<rav::Id, std::string>>
rav::RavennaNode::create_receiver(RavennaReceiver::Configuration initial_config) {
    auto work = [this, config = std::move(initial_config)]() mutable -> tl::expected<Id, std::string> {
        auto new_receiver = std::make_unique<RavennaReceiver>(rtsp_client_, rtp_receiver_, id_generator_.next());
        new_receiver->set_network_interface_config(network_interface_config_);
        auto result = new_receiver->set_configuration(std::move(config));
        if (!result) {
            RAV_ERROR("Failed to set receiver configuration: {}", result.error());
            return tl::unexpected(result.error());
        }
        const auto& it = receivers_.emplace_back(std::move(new_receiver));
        RAV_ASSERT(!nmos_node_.get_devices().empty(), "NMOS node must have at least one device");
        it->set_nmos_device_id(nmos_device_.id);
        it->set_nmos_node(&nmos_node_);
        for (const auto& s : subscribers_) {
            s->ravenna_receiver_added(*it);
        }
        return it->get_id();
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::remove_receiver(Id receiver_id) {
    auto work = [this, receiver_id]() mutable {
        for (auto it = receivers_.begin(); it != receivers_.end(); ++it) {
            if ((*it)->get_id() == receiver_id) {
                // Extend the lifetime until after the realtime context is updated:
                const std::unique_ptr<RavennaReceiver> tmp = std::move(*it);
                RAV_ASSERT(tmp != nullptr, "Receiver expected to be valid");
                receivers_.erase(it);
                for (const auto& s : subscribers_) {
                    s->ravenna_receiver_removed(receiver_id);
                }
                return;
            }
        }
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<tl::expected<void, std::string>>
rav::RavennaNode::update_receiver_configuration(Id receiver_id, RavennaReceiver::Configuration config) {
    auto work = [this, receiver_id, u = std::move(config)]() -> tl::expected<void, std::string> {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                return receiver->set_configuration(u);
            }
        }
        return tl::unexpected("Sender not found");
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<tl::expected<rav::Id, std::string>>
rav::RavennaNode::create_sender(RavennaSender::Configuration initial_config) {
    auto work = [this, initial_config]() mutable -> tl::expected<Id, std::string> {
        auto new_sender = std::make_unique<RavennaSender>(
            rtp_sender_, *advertiser_, rtsp_server_, ptp_instance_, id_generator_.next(), generate_unique_session_id()
        );
        if (initial_config.session_name.empty()) {
            initial_config.session_name = fmt::format("Sender {}", new_sender->get_session_id());
        }
        new_sender->set_network_interface_config(network_interface_config_);
        auto result = new_sender->set_configuration(initial_config);
        if (!result) {
            RAV_ERROR("Failed to set sender configuration: {}", result.error());
            return tl::unexpected(result.error());
        }
        const auto& it = senders_.emplace_back(std::move(new_sender));
        it->set_nmos_device_id(nmos_device_.id);
        it->set_nmos_node(&nmos_node_);
        for (const auto& s : subscribers_) {
            s->ravenna_sender_added(*it);
        }
        return it->get_id();
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::remove_sender(Id sender_id) {
    auto work = [this, sender_id]() mutable {
        for (auto it = senders_.begin(); it != senders_.end(); ++it) {
            if ((*it)->get_id() == sender_id) {
                // Extend the lifetime until after the realtime context is updated:
                const std::unique_ptr<RavennaSender> tmp = std::move(*it);
                RAV_ASSERT(tmp != nullptr, "Receiver expected to be valid");
                senders_.erase(it);  // It is empty by now
                for (const auto& s : subscribers_) {
                    s->ravenna_sender_removed(sender_id);
                }
                return;
            }
        }
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<tl::expected<void, std::string>>
rav::RavennaNode::update_sender_configuration(Id sender_id, RavennaSender::Configuration config) {
    auto work = [this, sender_id, u = std::move(config)]() -> tl::expected<void, std::string> {
        for (const auto& sender : senders_) {
            if (sender->get_id() == sender_id) {
                return sender->set_configuration(u);
            }
        }
        return tl::unexpected("Sender not found");
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<tl::expected<void, std::string>>
rav::RavennaNode::set_nmos_configuration(nmos::Node::Configuration update) {
    auto work = [this, u = std::move(update)]() -> tl::expected<void, std::string> {
        auto result = nmos_node_.set_configuration(u);
        if (!result) {
            return tl::unexpected(fmt::format("Failed to set nmos configuration: {}", result.error()));
        }
        nmos_device_.label = u.label;
        nmos_device_.description = u.description;
        if (!nmos_node_.add_or_update_device(nmos_device_)) {
            return tl::unexpected("Failed to update NMOS device configuration");
        }
        return {};
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<boost::uuids::uuid> rav::RavennaNode::get_nmos_device_id() {
    auto work = [this]() -> boost::uuids::uuid {
        return nmos_device_.id;
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::subscribe(Subscriber* subscriber) {
    RAV_ASSERT(subscriber != nullptr, "Subscriber must be valid");
    auto work = [this, subscriber] {
        if (!subscribers_.add(subscriber)) {
            RAV_ERROR("Failed to add subscriber to node");
            return;
        }
        if (!browser_.subscribe(subscriber)) {
            RAV_ERROR("Failed to add subscriber to browser");
            if (!subscribers_.remove(subscriber)) {
                RAV_ERROR("Failed to remove subscriber from node");
            }
            return;
        }
        for (const auto& receiver : receivers_) {
            subscriber->ravenna_receiver_added(*receiver);
        }
        for (const auto& sender : senders_) {
            subscriber->ravenna_sender_added(*sender);
        }
        subscriber->network_interface_config_updated(network_interface_config_);
        subscriber->nmos_node_config_updated(nmos_node_.get_configuration());
        subscriber->nmos_node_status_changed(nmos_node_.get_status(), nmos_node_.get_registry_info());
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::unsubscribe(Subscriber* subscriber) {
    RAV_ASSERT(subscriber != nullptr, "Subscriber must be valid");
    auto work = [this, subscriber] {
        if (!browser_.unsubscribe(subscriber)) {
            RAV_WARNING("Failed to remove subscriber from browser");
        }
        if (!subscribers_.remove(subscriber)) {
            RAV_WARNING("Failed to remove subscriber from node");
        }
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::subscribe_to_receiver(Id receiver_id, RavennaReceiver::Subscriber* subscriber) {
    auto work = [this, receiver_id, subscriber] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                if (!receiver->subscribe(subscriber)) {
                    RAV_WARNING("Already subscribed");
                }
                return;
            }
        }
        RAV_WARNING("Receiver not found");
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::unsubscribe_from_receiver(Id receiver_id, RavennaReceiver::Subscriber* subscriber) {
    auto work = [this, receiver_id, subscriber] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                if (!receiver->unsubscribe(subscriber)) {
                    RAV_WARNING("Not subscribed");
                }
                return;
            }
        }
        // Don't warn about not finding the receiver, as the receiver might have already been removed.
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::subscribe_to_sender(Id sender_id, RavennaSender::Subscriber* subscriber) {
    auto work = [this, sender_id, subscriber] {
        for (const auto& sender : senders_) {
            if (sender->get_id() == sender_id) {
                if (!sender->subscribe(subscriber)) {
                    RAV_WARNING("Already subscribed");
                }
                return;
            }
        }
        RAV_WARNING("Sender not found");
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::unsubscribe_from_sender(Id sender_id, RavennaSender::Subscriber* subscriber) {
    auto work = [this, sender_id, subscriber] {
        for (const auto& sender : senders_) {
            if (sender->get_id() == sender_id) {
                if (!sender->unsubscribe(subscriber)) {
                    RAV_WARNING("Not subscribed");
                }
                return;
            }
        }
        // Don't warn about not finding the sender, as the sender might have already been removed.
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::subscribe_to_ptp_instance(ptp::Instance::Subscriber* subscriber) {
    auto work = [this, subscriber] {
        if (!ptp_instance_.subscribe(subscriber)) {
            RAV_ERROR("Failed to add subscriber to PTP instance");
        }
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<void> rav::RavennaNode::unsubscribe_from_ptp_instance(ptp::Instance::Subscriber* subscriber) {
    auto work = [this, subscriber] {
        if (!ptp_instance_.unsubscribe(subscriber)) {
            RAV_ERROR("Failed to remove subscriber from PTP instance");
        }
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<std::optional<rav::sdp::SessionDescription>> rav::RavennaNode::get_sdp_for_receiver(Id receiver_id) {
    auto work = [this, receiver_id]() -> std::optional<sdp::SessionDescription> {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                return receiver->get_sdp();
            }
        }
        return std::nullopt;
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<std::optional<std::string>> rav::RavennaNode::get_sdp_text_for_receiver(Id receiver_id) {
    TRACY_ZONE_SCOPED;
    auto work = [this, receiver_id]() -> std::optional<std::string> {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                return receiver->get_sdp_text();
            }
        }
        return std::nullopt;
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::optional<uint32_t> rav::RavennaNode::read_data_realtime(
    const Id receiver_id, uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp,
    const std::optional<uint32_t> require_delay
) {
    TRACY_ZONE_SCOPED;
    return rtp_receiver_.read_data_realtime(receiver_id, buffer, buffer_size, at_timestamp, require_delay);
}

std::optional<uint32_t> rav::RavennaNode::read_audio_data_realtime(
    const Id receiver_id, const AudioBufferView<float>& output_buffer, const std::optional<uint32_t> at_timestamp,
    const std::optional<uint32_t> require_delay
) {
    TRACY_ZONE_SCOPED;
    return rtp_receiver_.read_audio_data_realtime(receiver_id, output_buffer, at_timestamp, require_delay);
}

bool rav::RavennaNode::send_data_realtime(
    const Id sender_id, const BufferView<const uint8_t> buffer, const uint32_t timestamp
) {
    return rtp_sender_.send_data_realtime(sender_id, buffer, timestamp);
}

bool rav::RavennaNode::send_audio_data_realtime(
    const Id sender_id, const AudioBufferView<const float>& buffer, const uint32_t timestamp
) {
    return rtp_sender_.send_audio_data_realtime(sender_id, buffer, timestamp);
}

std::future<void> rav::RavennaNode::set_network_interface_config(NetworkInterfaceConfig interface_config) {
    auto work = [this, config = std::move(interface_config)] {
        if (network_interface_config_ == config) {
            return;  // Nothing changed
        }

        network_interface_config_ = config;
        const auto array_of_addresses =
            network_interface_config_.get_array_of_interface_addresses<rtp::AudioSender::k_max_num_redundant_sessions>(
            );

        if (!rtp_receiver_.set_interfaces(array_of_addresses)) {
            RAV_ERROR("Failed to set network interfaces on rtp receiver");
        }

        for (const auto& receiver : receivers_) {
            receiver->set_network_interface_config(network_interface_config_);
        }

        if (!rtp_sender_.set_interfaces(array_of_addresses)) {
            RAV_ERROR("Failed to set network interface on rtp sender");
        }

        for (const auto& sender : senders_) {
            sender->set_network_interface_config(network_interface_config_);
        }

        nmos_node_.set_network_interface_config(config);

        // Add or update PTP ports based on the new configuration
        const auto addresses = network_interface_config_.get_interface_ipv4_addresses();
        for (auto& [rank, address] : addresses) {
            auto it = ptp_ports_.find(rank);
            if (it == ptp_ports_.end()) {
                auto port_number = ptp_instance_.add_port(address);
                if (!port_number) {
                    RAV_ERROR("Failed to add PTP port: {}", ptp::to_string(port_number.error()));
                    continue;
                }
                ptp_ports_[rank] = port_number.value();
            } else {
                if (!ptp_instance_.set_port_interface(it->second, address)) {
                    RAV_ERROR("Failed to set PTP port interface: {}", it->second);
                }
            }
        }

        // Remove PTP ports that are no longer in the configuration (by rank)
        for (auto it = ptp_ports_.begin(); it != ptp_ports_.end();) {
            if (addresses.find(it->first) == addresses.end()) {
                if (!ptp_instance_.remove_port(it->second)) {
                    RAV_ERROR("Failed to remove PTP port: {}", it->second);
                }
                it = ptp_ports_.erase(it);
            } else {
                ++it;
            }
        }

        for (const auto& subscriber : subscribers_) {
            subscriber->network_interface_config_updated(config);
        }

        RAV_INFO("{}", config.to_string());
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

bool rav::RavennaNode::is_maintenance_thread() const {
    return maintenance_thread_id_ == std::this_thread::get_id();
}

std::future<boost::json::object> rav::RavennaNode::to_boost_json() {
    auto work = [this] {
        auto senders = boost::json::array();
        for (const auto& sender : senders_) {
            senders.push_back(sender->to_boost_json());
        }
        auto receivers = boost::json::array();
        for (const auto& receiver : receivers_) {
            receivers.push_back(receiver->to_boost_json());
        }

        boost::json::object config {{
            "network_config",
            network_interface_config_.to_boost_json(),
        }};

        return boost::json::object {
            {"config", config},
            {"senders", senders},
            {"receivers", receivers},
            {"nmos_node", boost::json::object {{"configuration", nmos_node_.get_configuration().to_json()}}},
            {"nmos_device_id", boost::uuids::to_string(nmos_device_.id)},
        };
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

std::future<tl::expected<void, std::string>> rav::RavennaNode::restore_from_boost_json(const boost::json::value& json) {
    auto work = [this, json]() -> tl::expected<void, std::string> {
        try {
            // Configuration

            auto network_interface_config =
                NetworkInterfaceConfig::from_boost_json(json.at("config").at("network_config"));

            if (!network_interface_config) {
                return tl::unexpected(network_interface_config.error());
            }

            auto nmos_device_id = boost::uuids::string_generator()(json.at("nmos_device_id").as_string().c_str());

            // Senders

            auto senders = json.at("senders").as_array();
            std::vector<std::unique_ptr<RavennaSender>> new_senders;

            for (auto& sender : senders) {
                auto new_sender = std::make_unique<RavennaSender>(
                    rtp_sender_, *advertiser_, rtsp_server_, ptp_instance_, id_generator_.next(), 1
                );
                new_sender->set_network_interface_config(*network_interface_config);
                if (auto result = new_sender->restore_from_json(sender); !result) {
                    return tl::unexpected(result.error());
                }
                new_sender->set_nmos_device_id(nmos_device_id);
                new_senders.push_back(std::move(new_sender));
            }

            // Receivers

            auto receivers = json.at("receivers").as_array();
            std::vector<std::unique_ptr<RavennaReceiver>> new_receivers;

            for (auto& receiver : receivers) {
                auto new_receiver =
                    std::make_unique<RavennaReceiver>(rtsp_client_, rtp_receiver_, id_generator_.next());
                new_receiver->set_network_interface_config(*network_interface_config);
                if (auto result = new_receiver->restore_from_json(receiver); !result) {
                    return tl::unexpected(result.error());
                }
                new_receiver->set_nmos_device_id(nmos_device_id);
                new_receivers.push_back(std::move(new_receiver));
            }

            // TODO: Restoring the NMOS node can still fail, should this go below that?
            set_network_interface_config(*network_interface_config).wait();

            // NMOS Node

            auto nmos_node = json.try_at("nmos_node");
            if (nmos_node.has_value()) {
                auto nmos_node_object = nmos_node->as_object();
                auto config = nmos::Node::Configuration::from_json(nmos_node_object.at("configuration"));
                if (config.has_error()) {
                    return tl::unexpected(config.error());
                }

                nmos_node_.stop();
                if (!nmos_node_.remove_device(nmos_device_.id)) {
                    RAV_ERROR("Failed to remove NMOS device with ID: {}", boost::uuids::to_string(nmos_device_.id));
                }
                auto result = nmos_node_.set_configuration(*config);
                if (!result) {
                    return tl::unexpected(fmt::format("Failed to set NMOS node configuration: {}", result.error()));
                }
                nmos_device_.id = nmos_device_id;
                nmos_device_.label = config->label;
                nmos_device_.description = config->description;
                if (!nmos_node_.add_or_update_device(std::move(nmos_device_))) {
                    RAV_ERROR("Failed to add NMOS device to node");
                }
            } else {
                return tl::unexpected("No NMOS node state found in JSON");
            }

            // Swap senders

            for (const auto& sender : senders_) {
                sender->set_nmos_node(nullptr);
                for (auto* s : subscribers_) {
                    RAV_ASSERT(s != nullptr, "Subscriber must be valid");
                    s->ravenna_sender_removed(sender->get_id());
                }
            }

            std::swap(senders_, new_senders);

            for (const auto& sender : senders_) {
                sender->set_nmos_node(&nmos_node_);
                for (auto* s : subscribers_) {
                    RAV_ASSERT(s != nullptr, "Subscriber must be valid");
                    s->ravenna_sender_added(*sender);
                }
            }

            // Swap receivers

            for (const auto& receiver : receivers_) {
                receiver->set_nmos_node(nullptr);
                for (auto* s : subscribers_) {
                    RAV_ASSERT(s != nullptr, "Subscriber must be valid");
                    s->ravenna_receiver_removed(receiver->get_id());
                }
            }

            std::swap(receivers_, new_receivers);

            for (const auto& receiver : receivers_) {
                receiver->set_nmos_node(&nmos_node_);
                for (auto* s : subscribers_) {
                    RAV_ASSERT(s != nullptr, "Subscriber must be valid");
                    s->ravenna_receiver_added(*receiver);
                }
            }

        } catch (const std::exception& e) {
            return tl::unexpected(fmt::format("Failed to parse RavennaNode JSON: {}", e.what()));
        }

        return {};
    };
    return boost::asio::dispatch(io_context_, boost::asio::use_future(work));
}

uint32_t rav::RavennaNode::generate_unique_session_id() const {
    uint32_t highest_session_id = 0;
    for (auto& sender : senders_) {
        highest_session_id = std::max(highest_session_id, sender->get_session_id());
    }
    return highest_session_id + 1;
}

void rav::RavennaNode::do_maintenance() const {
    for (const auto& receiver : receivers_) {
        receiver->do_maintenance();
    }
}
