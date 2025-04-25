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

#include <utility>
#include "ravennakit/ravenna/ravenna_sender.hpp"

rav::RavennaNode::RavennaNode() :
    rtsp_server_(io_context_, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 0)), ptp_instance_(io_context_) {
    rtp_receiver_ = std::make_unique<rtp::Receiver>(udp_receiver_);
    advertiser_ = dnssd::Advertiser::create(io_context_);

    std::promise<std::thread::id> promise;
    auto f = promise.get_future();
    maintenance_thread_ = std::thread([this, p = std::move(promise)]() mutable {
        p.set_value(std::this_thread::get_id());
#if RAV_APPLE
        pthread_setname_np("ravenna_node_maintenance");
#endif
        while (true) {
            try {
                while (!io_context_.stopped()) {
                    io_context_.poll();
                }
                break;
            } catch (const std::exception& e) {
                RAV_ERROR("Unhandled exception in maintenance thread: {}", e.what());
            }
        }
    });
    maintenance_thread_id_ = f.get();
}

rav::RavennaNode::~RavennaNode() {
    io_context_.stop();
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
}

std::future<rav::Id> rav::RavennaNode::create_receiver(const RavennaReceiver::ConfigurationUpdate& initial_config) {
    auto work = [this, initial_config]() mutable {
        auto new_receiver = std::make_unique<RavennaReceiver>(
            io_context_, rtsp_client_, *rtp_receiver_, id_generator_.next(), initial_config
        );
        new_receiver->set_interfaces(config_.network_interfaces.get_interface_ipv4_addresses());
        const auto& it = receivers_.emplace_back(std::move(new_receiver));
        for (const auto& s : subscribers_) {
            s->ravenna_receiver_added(*it);
        }
        if (!update_realtime_shared_context()) {
            RAV_ERROR("Failed to update realtime shared context");
        }
        return it->get_id();
    };
    return asio::dispatch(io_context_, asio::use_future(work));
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
                if (!update_realtime_shared_context()) {
                    // If this happens we're out of luck, because the object will be deleted after this.
                    RAV_ERROR("Failed to update realtime shared context");
                }
                return;
            }
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<tl::expected<void, std::string>>
rav::RavennaNode::update_receiver_configuration(Id receiver_id, RavennaReceiver::ConfigurationUpdate update) {
    auto work = [this, receiver_id, u = std::move(update)]() -> tl::expected<void, std::string> {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                return receiver->set_configuration(u);
            }
        }
        return tl::unexpected("Sender not found");
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<rav::Id> rav::RavennaNode::create_sender(const RavennaSender::ConfigurationUpdate& initial_config) {
    auto work = [this, initial_config]() mutable {
        auto interface_address = config_.network_interfaces.get_interface_ipv4_address(Rank(0));
        auto new_sender = std::make_unique<RavennaSender>(
            io_context_, *advertiser_, rtsp_server_, ptp_instance_, id_generator_.next(), generate_unique_session_id(),
            interface_address, initial_config
        );
        const auto& it = senders_.emplace_back(std::move(new_sender));
        for (const auto& s : subscribers_) {
            s->ravenna_sender_added(*it);
        }
        if (!update_realtime_shared_context()) {
            RAV_ERROR("Failed to update realtime shared context");
        }
        return it->get_id();
    };
    return asio::dispatch(io_context_, asio::use_future(work));
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
                if (!update_realtime_shared_context()) {
                    // If this happens we're out of luck, because the object will be deleted after this.
                    RAV_ERROR("Failed to update realtime shared context");
                }
                return;
            }
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<tl::expected<void, std::string>>
rav::RavennaNode::update_sender_configuration(Id sender_id, RavennaSender::ConfigurationUpdate update) {
    auto work = [this, sender_id, u = std::move(update)]() -> tl::expected<void, std::string> {
        for (const auto& sender : senders_) {
            if (sender->get_id() == sender_id) {
                return sender->set_configuration(u);
            }
        }
        return tl::unexpected("Sender not found");
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::RavennaNode::subscribe(Subscriber* subscriber) {
    RAV_ASSERT(subscriber != nullptr, "Subscriber must be valid");
    auto work = [this, subscriber] {
        if (!subscribers_.add(subscriber)) {
            RAV_WARNING("Failed to add subscriber to node");
        }
        if (!browser_.subscribe(subscriber)) {
            RAV_WARNING("Failed to add subscriber to browser");
        }
        for (const auto& receiver : receivers_) {
            subscriber->ravenna_receiver_added(*receiver);
        }
        for (const auto& sender : senders_) {
            subscriber->ravenna_sender_added(*sender);
        }
        subscriber->network_interface_config_updated(config_.network_interfaces);
    };
    return asio::dispatch(io_context_, asio::use_future(work));
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
    return asio::dispatch(io_context_, asio::use_future(work));
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
    return asio::dispatch(io_context_, asio::use_future(work));
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
    return asio::dispatch(io_context_, asio::use_future(work));
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
    return asio::dispatch(io_context_, asio::use_future(work));
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
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::RavennaNode::subscribe_to_ptp_instance(ptp::Instance::Subscriber* subscriber) {
    auto work = [this, subscriber] {
        if (!ptp_instance_.subscribe(subscriber)) {
            RAV_ERROR("Failed to add subscriber to PTP instance");
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::RavennaNode::unsubscribe_from_ptp_instance(ptp::Instance::Subscriber* subscriber) {
    auto work = [this, subscriber] {
        if (!ptp_instance_.unsubscribe(subscriber)) {
            RAV_ERROR("Failed to remove subscriber from PTP instance");
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<rav::rtp::AudioReceiver::SessionStats> rav::RavennaNode::get_stats_for_receiver(Id receiver_id, Rank rank) {
    auto work = [this, receiver_id, rank] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                return receiver->get_stream_stats(rank);
            }
        }
        return rtp::AudioReceiver::SessionStats {};
    };
    return asio::dispatch(io_context_, asio::use_future(work));
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
    return asio::dispatch(io_context_, asio::use_future(work));
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
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::optional<uint32_t> rav::RavennaNode::read_data_realtime(
    const Id receiver_id, uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    const auto lock = realtime_shared_context_.lock_realtime();

    for (auto* receiver : lock->receivers) {
        if (receiver->get_id() == receiver_id) {
            return receiver->read_data_realtime(buffer, buffer_size, at_timestamp);
        }
    }

    return std::nullopt;
}

std::optional<uint32_t> rav::RavennaNode::read_audio_data_realtime(
    const Id receiver_id, const AudioBufferView<float>& output_buffer, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    const auto context = realtime_shared_context_.lock_realtime();

    for (auto* receiver : context->receivers) {
        if (receiver->get_id() == receiver_id) {
            return receiver->read_audio_data_realtime(output_buffer, at_timestamp);
        }
    }

    return std::nullopt;
}

bool rav::RavennaNode::send_data_realtime(
    const Id sender_id, const BufferView<const uint8_t> buffer, const uint32_t timestamp
) {
    const auto lock = realtime_shared_context_.lock_realtime();

    for (auto* sender : lock->senders) {
        if (sender->get_id() == sender_id) {
            return sender->send_data_realtime(buffer, timestamp);
        }
    }

    return false;
}

bool rav::RavennaNode::send_audio_data_realtime(
    const Id sender_id, const AudioBufferView<const float>& buffer, const uint32_t timestamp
) {
    const auto lock = realtime_shared_context_.lock_realtime();

    for (auto* sender : lock->senders) {
        if (sender->get_id() == sender_id) {
            return sender->send_audio_data_realtime(buffer, timestamp);
        }
    }

    return false;
}

std::future<void>
rav::RavennaNode::set_network_interface_config(RavennaConfig::NetworkInterfaceConfig interface_config) {
    auto work = [this, config = std::move(interface_config)] {
        if (config_.network_interfaces == config) {
            return;  // Nothing changed
        }

        config_.network_interfaces = std::move(config);
        const auto addresses = config_.network_interfaces.get_interface_ipv4_addresses();

        for (const auto& receiver : receivers_) {
            receiver->set_interfaces(addresses);
        }

        const auto first_interface = addresses.begin();
        if (first_interface != addresses.end()) {
            for (const auto& sender : senders_) {
                sender->set_interface(first_interface->second);
            }

            if (ptp_instance_.get_port_count() == 0) {
                ptp_instance_.add_port(first_interface->second);
            } else {
                ptp_instance_.set_port_interface(0, first_interface->second);
            }
        }

        for (const auto& subscriber : subscribers_) {
            subscriber->network_interface_config_updated(config);
        }

        RAV_INFO("{}", config.to_string());
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

bool rav::RavennaNode::is_maintenance_thread() const {
    return maintenance_thread_id_ == std::this_thread::get_id();
}

std::future<nlohmann::json> rav::RavennaNode::to_json() {
    auto work = [this] {
        auto senders = nlohmann::json::array();
        for (const auto& sender : senders_) {
            senders.push_back(sender->to_json());
        }
        auto receivers = nlohmann::json::array();
        for (const auto& receiver : receivers_) {
            receivers.push_back(receiver->to_json());
        }
        nlohmann::json root;
        root["config"] = config_.to_json();
        root["senders"] = senders;
        root["receivers"] = receivers;
        return root;
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<tl::expected<void, std::string>> rav::RavennaNode::restore_from_json(const nlohmann::json& json) {
    auto work = [this, json]() -> tl::expected<void, std::string> {
        try {
            auto ravenna_config = RavennaConfig::from_json(json.at("config"));
            if (!ravenna_config) {
                return tl::unexpected(ravenna_config.error());
            }

            set_network_interface_config(ravenna_config->network_interfaces).wait();

            auto interface_addresses = ravenna_config->network_interfaces.get_interface_ipv4_addresses();

            {
                auto senders = json.at("senders");
                std::vector<std::unique_ptr<RavennaSender>> new_senders;

                for (auto& sender : senders) {
                    auto config = RavennaSender::ConfigurationUpdate::from_json(sender.at("configuration"));
                    if (!config) {
                        return tl::unexpected(config.error());
                    }
                    auto session_id = sender.at("session_id").get<uint32_t>();
                    auto interface_address = config_.network_interfaces.get_interface_ipv4_address(Rank::primary());
                    auto new_sender = std::make_unique<RavennaSender>(
                        io_context_, *advertiser_, rtsp_server_, ptp_instance_, id_generator_.next(), session_id,
                        interface_address, *config
                    );
                    new_senders.push_back(std::move(new_sender));
                }

                for (const auto& sender : senders_) {
                    for (auto* s : subscribers_) {
                        RAV_ASSERT(s != nullptr, "Subscriber must be valid");
                        s->ravenna_sender_removed(sender->get_id());
                    }
                }

                std::swap(senders_, new_senders);

                for (const auto& sender : senders_) {
                    for (auto* s : subscribers_) {
                        RAV_ASSERT(s != nullptr, "Subscriber must be valid");
                        s->ravenna_sender_added(*sender);
                    }
                }
            }

            {
                auto receivers = json.at("receivers");
                std::vector<std::unique_ptr<RavennaReceiver>> new_receivers;

                for (auto& receiver : receivers) {
                    auto config = RavennaReceiver::ConfigurationUpdate::from_json(receiver.at("configuration"));
                    if (!config) {
                        return tl::unexpected(config.error());
                    }
                    auto new_receiver = std::make_unique<RavennaReceiver>(
                        io_context_, rtsp_client_, *rtp_receiver_, id_generator_.next(), *config
                    );

                    new_receiver->set_interfaces(interface_addresses);
                    new_receivers.push_back(std::move(new_receiver));
                }

                for (const auto& receiver : receivers_) {
                    for (auto* s : subscribers_) {
                        RAV_ASSERT(s != nullptr, "Subscriber must be valid");
                        s->ravenna_receiver_removed(receiver->get_id());
                    }
                }

                std::swap(receivers_, new_receivers);

                for (const auto& receiver : receivers_) {
                    for (auto* s : subscribers_) {
                        RAV_ASSERT(s != nullptr, "Subscriber must be valid");
                        s->ravenna_receiver_added(*receiver);
                    }
                }
            }

        } catch (const std::exception& e) {
            return tl::unexpected(fmt::format("Failed to parse RavennaNode JSON: {}", e.what()));
        }

        if (!update_realtime_shared_context()) {
            RAV_ERROR("Failed to update realtime shared context");
        }

        return {};
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

bool rav::RavennaNode::update_realtime_shared_context() {
    auto new_context = std::make_unique<realtime_shared_context>();
    for (auto& receiver : receivers_) {
        new_context->receivers.emplace_back(receiver.get());
    }
    for (auto& sender : senders_) {
        new_context->senders.emplace_back(sender.get());
    }
    return realtime_shared_context_.update(std::move(new_context));
}

uint32_t rav::RavennaNode::generate_unique_session_id() const {
    uint32_t highest_session_id = 0;
    for (auto& sender : senders_) {
        highest_session_id = std::max(highest_session_id, sender->get_session_id());
    }
    return highest_session_id + 1;
}
