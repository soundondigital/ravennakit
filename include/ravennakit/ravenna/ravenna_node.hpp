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

/**
 * This example demonstrates how to set up a ravenna_node. This the most high level class available for implementing
 * RAVENNA and will eventually support everything needed to implement a RAVENNA node. At this stage the node is capable
 * of receiving streams.
 */

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
    class subscriber: public ravenna_browser::subscriber {
      public:
        ~subscriber() override = default;

        /**
         * Called when a receiver is added to the node, or when subscribing.
         * @param receiver The id of the receiver.
         */
        virtual void ravenna_receiver_added([[maybe_unused]] const ravenna_receiver& receiver) {}

        /**
         * Called when a receiver is removed from the node.
         * @param receiver_id The id of the receiver.
         */
        virtual void ravenna_receiver_removed([[maybe_unused]] id receiver_id) {}
    };

    explicit ravenna_node(rtp_receiver::configuration config);
    ~ravenna_node();

    /**
     * Creates a new receiver for the given session.
     * @param session_name The name of the session to create a receiver for.
     * @return The ID of the created receiver, which might be invalid if the receiver couldn't be created.
     */
    [[nodiscard]] std::future<id> create_receiver(const std::string& session_name);

    /**
     * Removes the receiver with the given id.
     * @param receiver_id The id of the receiver to remove.
     * @return A future that will be set when the operation is complete.
     */
    std::future<void> remove_receiver(id receiver_id);

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
     * @param receiver_id The id of the stream to add the subscriber to.
     * @param subscriber The subscriber to add.
     * @return A future that will be set when the operation is complete.
     */
    std::future<void> add_receiver_subscriber(id receiver_id, rtp_stream_receiver::subscriber* subscriber);

    /**
     * Removes a subscriber from the receiver with the given id.
     * @param receiver_id The id of the stream to remove the subscriber from.
     * @param subscriber The subscriber to remove.
     * @return A future that will be set when the operation is complete.
     */
    std::future<void> remove_receiver_subscriber(id receiver_id, rtp_stream_receiver::subscriber* subscriber);

    /**
     * Get the packet statistics for the given stream, if the stream for the given ID exists.
     * @param receiver_id The ID of the stream to get the packet statistics for.
     * @return The packet statistics for the stream, or an empty structure if the stream doesn't exist.
     */
    std::future<rtp_stream_receiver::stream_stats> get_stats_for_receiver(id receiver_id);

    /**
     * Calls back with the ravenna receiver for the given receiver id. If the stream is not found, the callback will not
     * be called and the future will be set to false.
     * @param receiver_id The id of the receiver to get the receiver for.
     * @param update_function The function to call with the receiver.
     */
    std::future<bool> get_receiver(id receiver_id, std::function<void(ravenna_receiver&)> update_function);

    /**
     * Get the SDP for the receiver with the given id.
     * @param receiver_id The id of the receiver to get the SDP for.
     * @return The SDP for the receiver.
     */
    std::future<std::optional<sdp::session_description>> get_sdp_for_receiver(id receiver_id);

    /**
     * Get the SDP text for the receiver with the given id. This is the original SDP text as received from the server,
     * and might contain things which haven't been parsed into the session_description.
     * @param receiver_id The id of the receiver to get the SDP text for.
     * @return The SDP text for the receiver.
     */
    std::future<std::optional<std::string>> get_sdp_text_for_receiver(id receiver_id);

    /**
     * Reads the data from the receiver with the given id.
     * @param receiver_id The id of the receiver to read data from.
     * @param at_timestamp The timestamp to read at.
     * @param buffer The buffer to read the data into.
     * @param buffer_size The size of the buffer.
     * @return True if the data was read, or false if the data couldn't be read.
     */
    bool realtime_read_data(id receiver_id, uint32_t at_timestamp, uint8_t* buffer, size_t buffer_size);

  private:
    asio::io_context io_context_;
    std::thread maintenance_thread_;
    asio::ip::address interface_address;

    ravenna_browser browser_ {io_context_};
    ravenna_rtsp_client rtsp_client_ {io_context_, browser_};
    std::unique_ptr<rtp_receiver> rtp_receiver_;

    std::vector<std::unique_ptr<ravenna_receiver>> receivers_;
    subscriber_list<subscriber> subscribers_;
};

}  // namespace rav
