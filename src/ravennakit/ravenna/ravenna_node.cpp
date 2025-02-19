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
        io_context_, asio::use_future([this, session_name]() mutable {
            const auto& it = receivers_.emplace_back(std::make_unique<ravenna_receiver>(rtsp_client_, rtp_receiver_));
            it->set_session_name(session_name);
            for (const auto& s : subscribers_) {
                s->on_receiver_updated(it->get_id());
            }
            return it->get_id();
        })
    );
}

std::future<void> rav::ravenna_node::subscribe(subscriber* subscriber) {
    return asio::dispatch(io_context_, asio::use_future([this, subscriber] {
                              if (!subscribers_.add(subscriber)) {
                                  RAV_WARNING("Already subscribed");
                              }
                              browser_.subscribe(subscriber);
                          }));
}

std::future<void> rav::ravenna_node::unsubscribe(subscriber* subscriber) {
    return asio::dispatch(io_context_, asio::use_future([this, subscriber] {
                              browser_.unsubscribe(subscriber);
                              if (!subscribers_.remove(subscriber)) {
                                  RAV_WARNING("Not subscribed");
                              }
                          }));
}

std::future<rav::rtp_stream_receiver::stream_stats> rav::ravenna_node::get_stats_for_stream(id stream_id) {
    return asio::dispatch(io_context_, asio::use_future([this, stream_id] {
                              for (const auto& receiver : receivers_) {
                                  if (receiver->get_id() == stream_id) {
                                      return receiver->get_session_stats();
                                  }
                              }
                              return rtp_stream_receiver::stream_stats {};
                          }));
}
