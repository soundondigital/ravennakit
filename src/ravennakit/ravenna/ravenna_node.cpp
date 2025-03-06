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

rav::ravenna_node::~ravenna_node() {
    io_context_.stop();
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
}

std::future<rav::id> rav::ravenna_node::create_receiver(const std::string& session_name) {
    auto work = [this, session_name]() mutable {
        const auto& it = receivers_.emplace_back(std::make_unique<ravenna_receiver>(rtsp_client_, *rtp_receiver_));
        it->subscribe_to_session(session_name);
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
    RAV_ASSERT(subscriber != nullptr, "Subscriber must be valid");
    auto work = [this, subscriber] {
        if (!subscribers_.add(subscriber)) {
            RAV_WARNING("Already subscribed");
        }
        subscriber->set_ravenna_browser(&browser_);
        for (const auto& receiver : receivers_) {
            subscriber->ravenna_receiver_added(*receiver);
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void> rav::ravenna_node::remove_subscriber(subscriber* subscriber) {
    RAV_ASSERT(subscriber != nullptr, "Subscriber must be valid");
    auto work = [this, subscriber] {
        subscriber->set_ravenna_browser(nullptr);
        if (!subscribers_.remove(subscriber)) {
            RAV_WARNING("Not subscribed");
        }
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<bool> rav::ravenna_node::add_receiver(ravenna_receiver* receiver) {
    RAV_ASSERT(receiver != nullptr, "Receiver must be valid");
    auto work = [this, receiver] {
        if (receivers_list_.add(receiver)) {
            receiver->set_ravenna_rtsp_client(&rtsp_client_);
            return true;
        }
        return false;
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<bool> rav::ravenna_node::remove_receiver(ravenna_receiver* receiver) {
    RAV_ASSERT(receiver != nullptr, "Receiver must be valid");
    auto work = [this, receiver] {
        if (receivers_list_.remove(receiver)) {
            receiver->set_ravenna_rtsp_client(nullptr);
            return true;
        }
        return false;
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void>
rav::ravenna_node::add_receiver_subscriber(id receiver_id, rtp_stream_receiver::subscriber* subscriber) {
    auto work = [this, receiver_id, subscriber] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                subscriber->set_rtp_stream_receiver(receiver.get());
                return;
            }
        }
        RAV_WARNING("Stream not found");
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void>
rav::ravenna_node::remove_receiver_subscriber(id receiver_id, rtp_stream_receiver::subscriber* subscriber) {
    auto work = [this, receiver_id, subscriber] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                subscriber->set_rtp_stream_receiver(nullptr);
                return;
            }
        }
        // Don't warn about not finding the stream, as the stream might have already been removed.
    };
    return asio::dispatch(io_context_, asio::use_future(work));
}

std::future<void>
rav::ravenna_node::add_receiver_data_callback(id receiver_id, rtp_stream_receiver::data_callback* callback) {
    auto work = [this, receiver_id, callback] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                if (!receiver->add_data_callback(callback)) {
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
rav::ravenna_node::remove_receiver_data_callback(id receiver_id, rtp_stream_receiver::data_callback* callback) {
    auto work = [this, receiver_id, callback] {
        for (const auto& receiver : receivers_) {
            if (receiver->get_id() == receiver_id) {
                if (!receiver->remove_data_callback(callback)) {
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

bool rav::ravenna_node::realtime_read_data(
    const id receiver_id, const uint32_t at_timestamp, uint8_t* buffer, const size_t buffer_size
) {
    // TODO: Synchronize with maintenance_thread_

    for (const auto& receiver : receivers_) {
        if (receiver->get_id() == receiver_id) {
            return receiver->realtime_read_data(at_timestamp, buffer, buffer_size);
        }
    }
    return false;
}

bool rav::ravenna_node::realtime_read_audio_data(
    const id receiver_id, const std::optional<uint32_t> at_timestamp, const audio_buffer_view<float>& output_buffer
) {
    // TODO: Synchronize with maintenance_thread_

    if (!at_timestamp.has_value()) {
        // TODO: Try to fill the timestamp with the current PTP time
    }

    for (const auto& receiver : receivers_) {
        if (receiver->get_id() == receiver_id) {
            return receiver->realtime_read_audio_data(at_timestamp, output_buffer);
        }
    }
    return false;
}

bool rav::ravenna_node::is_maintenance_thread() const {
    return maintenance_thread_id_ == std::this_thread::get_id();
}
