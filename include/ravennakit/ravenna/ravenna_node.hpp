/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "ravenna_browser.hpp"
#include "ravenna_rtsp_client.hpp"
#include "ravenna_receiver.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

#include <string>

namespace rav {

/**
 * This class contains all the components to act like a RAVENNA node as specified in the RAVENNA protocol.
 */
class ravenna_node {
  public:
    /**
     * Base class for classes which want to receive updates from the ravenna node.
     */
    class subscriber : public ravenna_browser::subscriber {
      public:
        ~subscriber() override = default;

        /**
         * Called when received with id was updated.
         * @param receiver The id of the receiver.
         */
        virtual void on_receiver_updated([[maybe_unused]] const ravenna_receiver& receiver) {}
    };

    ravenna_node();
    ~ravenna_node();

    /**
     * Creates a new receiver for the given session.
     * @param session_name The name of the session to create a receiver for.
     * @return The ID of the created receiver, which might be invalid if the receiver couldn't be created.
     */
    [[nodiscard]] std::future<id> create_receiver(const std::string& session_name);

    /**
     * Adds a subscriber to the node.
     * This method can be called from any thread, and will wait until the operation is complete.
     * @param subscriber The subscriber to add.
     */
    [[nodiscard]] std::future<void> add_subscriber(subscriber* subscriber);

    /**
     * Removes a subscriber from the node.
     * This method can be called from any thread, and will wait until the operation is complete.
     * @param subscriber The subscriber to remove.
     */
    [[nodiscard]] std::future<void> remove_subscriber(subscriber* subscriber);

    /**
     * Adds a subscriber to the receiver with the given id.
     * @param stream_id The id of the stream to add the subscriber to.
     * @param subscriber The subscriber to add.
     * @return A future that will be set when the operation is complete.
     */
    std::future<void> add_receiver_subscriber(id stream_id, rtp_stream_receiver::subscriber* subscriber);

    /**
     * Removes a subscriber from the receiver with the given id.
     * @param stream_id The id of the stream to remove the subscriber from.
     * @param subscriber The subscriber to remove.
     * @return A future that will be set when the operation is complete.
     */
    std::future<void> remove_stream_subscriber(id stream_id, rtp_stream_receiver::subscriber* subscriber);

    /**
     * Get the packet statistics for the given stream, if the stream for the given ID exists.
     * @param stream_id The ID of the stream to get the packet statistics for.
     * @return The packet statistics for the stream, or an empty structure if the stream doesn't exist.
     */
    std::future<rtp_stream_receiver::stream_stats> get_stats_for_stream(id stream_id);

  private:
    asio::io_context io_context_;
    std::thread maintenance_thread_;

    ravenna_browser browser_ {io_context_};
    ravenna_rtsp_client rtsp_client_ {io_context_, browser_};
    rtp_receiver rtp_receiver_ {io_context_, rtp_receiver::configuration {}};  // TODO: Make configuration configurable.

    std::vector<std::unique_ptr<ravenna_receiver>> receivers_;
    subscriber_list<subscriber> subscribers_;
};

}  // namespace rav
