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

rav::RavennaNode::RavennaNode(rtp::Receiver::Configuration config) {
    rtp_receiver_ = std::make_unique<rtp::Receiver>(io_context_, std::move(config));

    std::promise<std::thread::id> promise;
    auto f = promise.get_future();
    maintenance_thread_ = std::thread([this, p = std::move(promise)]() mutable {
        p.set_value(std::this_thread::get_id());
#if RAV_APPLE
        pthread_setname_np("ravenna_node_maintenance");
#endif
        try {
            io_context_.run();
        } catch (const std::exception& e) {
            RAV_ERROR("Exception in maintenance thread: {}", e.what());
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

std::future<rav::id> rav::RavennaNode::create_receiver(const std::string& session_name) {
    auto work = [this, session_name]() mutable {
        auto new_receiver = std::make_unique<RavennaReceiver>(rtsp_client_, *rtp_receiver_);
        if (!new_receiver->subscribe_to_session(session_name)) {
            RAV_ERROR("Failed to subscribe to session: {}", session_name);
            return id {};
        }
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

std::future<void> rav::RavennaNode::remove_receiver(id receiver_id) {
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
                    // If this happens we're out of luck, because the object will be deleted.
                    RAV_ERROR("Failed to update realtime shared context");
                }
                return;
            }
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<bool> rav::RavennaNode::set_receiver_delay(id receiver_id, uint32_t delay_samples) {
    auto work = [this, receiver_id, delay_samples] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                receiver->set_delay(delay_samples);
                return true;
            }
        }
        return false;
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::RavennaNode::subscribe(Subscriber* subscriber_to_add) {
    RAV_ASSERT(subscriber_to_add != nullptr, "Subscriber must be valid");
    auto work = [this, subscriber_to_add] {
        if (!subscribers_.add(subscriber_to_add)) {
            RAV_WARNING("Failed to add subscriber to node");
        }
        if (!browser_.subscribe(subscriber_to_add)) {
            RAV_WARNING("Failed to add subscriber to browser");
        }
        for (const auto& receiver : receivers_) {
            subscriber_to_add->ravenna_receiver_added(*receiver);
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::RavennaNode::unsubscribe(Subscriber* subscriber_to_remove) {
    RAV_ASSERT(subscriber_to_remove != nullptr, "Subscriber must be valid");
    auto work = [this, subscriber_to_remove] {
        if (!browser_.unsubscribe(subscriber_to_remove)) {
            RAV_WARNING("Failed to remove subscriber from browser");
        }
        if (!subscribers_.remove(subscriber_to_remove)) {
            RAV_WARNING("Failed to remove subscriber from node");
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void>
rav::RavennaNode::subscribe_to_receiver(id receiver_id, rtp::StreamReceiver::Subscriber* subscriber_to_add) {
    auto work = [this, receiver_id, subscriber_to_add] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                if (!receiver->subscribe(subscriber_to_add)) {
                    RAV_WARNING("Already subscribed");
                }
                return;
            }
        }
        RAV_WARNING("Stream not found");
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void>
rav::RavennaNode::unsubscribe_from_receiver(id receiver_id, rtp::StreamReceiver::Subscriber* subscriber_to_remove) {
    auto work = [this, receiver_id, subscriber_to_remove] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                if (!receiver->unsubscribe(subscriber_to_remove)) {
                    RAV_WARNING("Not subscribed");
                }
                return;
            }
        }
        // Don't warn about not finding the stream, as the stream might have already been removed.
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<rav::rtp::StreamReceiver::StreamStats> rav::RavennaNode::get_stats_for_receiver(id receiver_id) {
    auto work = [this, receiver_id] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                return receiver->get_session_stats();
            }
        }
        return rtp::StreamReceiver::StreamStats {};
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<bool>
rav::RavennaNode::get_receiver(id receiver_id, std::function<void(RavennaReceiver&)> update_function) {
    RAV_ASSERT(update_function != nullptr, "Function must be valid");
    auto work = [this, receiver_id, f = std::move(update_function)] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                f(*receiver);
                return true;
            }
        }
        return false;
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<std::optional<rav::sdp::session_description>> rav::RavennaNode::get_sdp_for_receiver(id receiver_id) {
    auto work = [this, receiver_id]() -> std::optional<sdp::session_description> {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                return receiver->get_sdp();
            }
        }
        return std::nullopt;
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<std::optional<std::string>> rav::RavennaNode::get_sdp_text_for_receiver(id receiver_id) {
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
    const id receiver_id, uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp
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
    const id receiver_id, const audio_buffer_view<float>& output_buffer, const std::optional<uint32_t> at_timestamp
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

bool rav::RavennaNode::is_maintenance_thread() const {
    return maintenance_thread_id_ == std::this_thread::get_id();
}

bool rav::RavennaNode::update_realtime_shared_context() {
    auto new_context = std::make_unique<realtime_shared_context>();
    for (auto& receiver : receivers_) {
        new_context->receivers.emplace_back(receiver.get());
    }
    return realtime_shared_context_.update(std::move(new_context));
}
