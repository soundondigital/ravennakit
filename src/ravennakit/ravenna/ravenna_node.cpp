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

rav::ravenna_node::ravenna_node() {
    maintenance_thread_ = std::thread([this] {
        io_context_.run();
    });
}

rav::ravenna_node::~ravenna_node() {
    io_context_.stop();
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
}

std::future<rav::id> rav::ravenna_node::create_receiver(const std::string& session_name) {
    return asio::dispatch(
        io_context_, std::packaged_task<rav::id()>([this, session_name]() mutable {
            const auto& it = receivers_.emplace_back(std::make_unique<ravenna_receiver>(rtsp_client_, rtp_receiver_));
            it->set_session_name(session_name);
            for (const auto& s : subscribers_) {
                s->on_receiver_updated(it->get_id());
            }
            return it->get_id();
        })
    );
}

std::future<void> rav::ravenna_node::subscribe_to_browser(ravenna_browser::subscriber* subscriber) {
    return asio::dispatch(io_context_, std::packaged_task<void()>([this, subscriber] {
                              browser_.subscribe(subscriber);
                          }));
}

std::future<void> rav::ravenna_node::unsubscribe_from_browser(ravenna_browser::subscriber* subscriber) {
    return asio::dispatch(io_context_, std::packaged_task<void()>([this, subscriber] {
                              browser_.unsubscribe(subscriber);
                          }));
}
