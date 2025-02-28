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

rav::ravenna_node::ravenna_node(rtp_receiver::configuration config) {
    rtp_receiver_ = std::make_unique<rtp_receiver>(io_context_, std::move(config));

    maintenance_thread_ = std::thread([this] {
        try {
            io_context_.run();
        } catch (const std::exception& e) {
            RAV_ERROR("Exception in maintenance thread: {}", e.what());
        }
    });
}

rav::ravenna_node::~ravenna_node() {
    io_context_.stop();
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
}

std::future<rav::id> rav::ravenna_node::create_receiver(const std::string& session_name) {
    auto work = [this, session_name]() mutable {
        const auto& it = receivers_.emplace_back(std::make_unique<ravenna_receiver>(rtsp_client_, *rtp_receiver_));
        it->set_session_name(session_name);
        for (const auto& s : subscribers_) {
            s->ravenna_receiver_added(*it);
        }
        return it->get_id();
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::ravenna_node::remove_receiver(id receiver_id) {
    auto work = [this, receiver_id]() mutable {
        for (auto it = receivers_.begin(); it != receivers_.end(); ++it) {
            if ((*it)->get_id() == receiver_id) {
                receivers_.erase(it);
                for (const auto& s : subscribers_) {
                    s->ravenna_receiver_removed(receiver_id);
                }
                return;
            }
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::ravenna_node::add_subscriber(subscriber* subscriber) {
    auto work = [this, subscriber] {
        if (!subscribers_.add(subscriber)) {
            RAV_WARNING("Already subscribed");
        }
        browser_.subscribe(subscriber);
        for (const auto& receiver : receivers_) {
            subscriber->ravenna_receiver_added(*receiver);
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::ravenna_node::remove_subscriber(subscriber* subscriber) {
    auto work = [this, subscriber] {
        browser_.unsubscribe(subscriber);
        if (!subscribers_.remove(subscriber)) {
            RAV_WARNING("Not subscribed");
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void>
rav::ravenna_node::add_receiver_subscriber(id receiver_id, rtp_stream_receiver::subscriber* subscriber) {
    auto work = [this, receiver_id, subscriber] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                if (!receiver->add_subscriber(subscriber)) {
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
rav::ravenna_node::remove_stream_subscriber(id receiver_id, rtp_stream_receiver::subscriber* subscriber) {
    auto work = [this, receiver_id, subscriber] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                if (!receiver->remove_subscriber(subscriber)) {
                    RAV_WARNING("Not subscribed");
                }
                return;
            }
        }
        // Don't warn about not finding the stream, as the stream might have already been removed.
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<rav::rtp_stream_receiver::stream_stats> rav::ravenna_node::get_stats_for_receiver(id receiver_id) {
    auto work = [this, receiver_id] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                return receiver->get_session_stats();
            }
        }
        return rtp_stream_receiver::stream_stats {};
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<bool>
rav::ravenna_node::get_receiver(id receiver_id, std::function<void(ravenna_receiver&)> update_function) {
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

std::future<std::optional<rav::sdp::session_description>> rav::ravenna_node::get_sdp_for_receiver(id receiver_id) {
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

std::future<std::optional<std::string>> rav::ravenna_node::get_sdp_text_for_receiver(id receiver_id) {
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
