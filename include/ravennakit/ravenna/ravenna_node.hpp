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
#include "ravenna_sender.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/sync/realtime_shared_object.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/rtp/detail/rtp_sender.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"

#include <string>

/**
 * Little helper macro to assert that the current thread is the maintenance thread of given node.
 * Done as macro to keep the location information.
 * @param node The node to check the maintenance thread for.
 */
#define RAV_ASSERT_NODE_MAINTENANCE_THREAD(node) RAV_ASSERT(node.is_maintenance_thread(), "Not on maintenance thread")

namespace rav {

/**
 * This class contains all the components to act like a RAVENNA node as specified in the RAVENNA protocol.
 */
class RavennaNode {
  public:
    /**
     * Base class for classes which want to receive updates from the ravenna node.
     */
    class Subscriber: public RavennaBrowser::Subscriber {
      public:
        ~Subscriber() override = default;

        /**
         * Called when a receiver is added to the node, or when subscribing.
         * Called from the maintenance thread.
         * @param receiver The id of the receiver.
         */
        virtual void ravenna_receiver_added([[maybe_unused]] const RavennaReceiver& receiver) {}

        /**
         * Called when a receiver is removed from the node.
         * Called from the maintenance thread.
         * @param receiver_id The id of the receiver.
         */
        virtual void ravenna_receiver_removed([[maybe_unused]] Id receiver_id) {}

        /**
         * Called when a sender is added to the node, or when subscribing.
         * Called from the maintenance thread.
         * @param sender The id of the sender.
         */
        virtual void ravenna_sender_added([[maybe_unused]] const RavennaSender& sender) {}

        /**
         * Called when a sender is removed from the node.
         * Called from the maintenance thread.
         * @param sender_id The id of the sender.
         */
        virtual void ravenna_sender_removed([[maybe_unused]] Id sender_id) {}
    };

    explicit RavennaNode(asio::ip::address_v4 interface_address);
    ~RavennaNode();

    /**
     * Creates a new receiver for the given session.
     * @param session_name The name of the session to create a receiver for.
     * @return The ID of the created receiver, which might be invalid if the receiver couldn't be created.
     */
    [[nodiscard]] std::future<Id> create_receiver(const std::string& session_name);

    /**
     * Removes the receiver with the given id.
     * @param receiver_id The id of the receiver to remove.
     * @return A future that will be set when the operation is complete.
     */
    [[nodiscard]] std::future<void> remove_receiver(Id receiver_id);

    /**
     * Sets the delay for the receiver with the given id.
     * @param receiver_id The id of the receiver to set the delay for.
     * @param delay_samples The delay in samples.
     * @return A future that will be set when the operation is complete. True if the delay was set, false if the
     * receiver was not found.
     */
    [[nodiscard]] std::future<bool> set_receiver_delay(Id receiver_id, uint32_t delay_samples);

    /**
     * Creates a sender for the given session.
     * @return The ID of the created sender, which might be invalid if the sender couldn't be created.
     */
    std::future<Id> create_sender(const RavennaSender::ConfigurationUpdate& initial_config = {});

    /**
     * Removes the sender with the given id.
     * @param sender_id The id of the sender to remove.
     * @return A future that will be set when the operation is complete.
     */
    std::future<void> remove_sender(Id sender_id);

    /**
     * Updates the configuration of the sender with the given id.
     * @param sender_id
     * @param update The configuration changes to apply.
     * @return A future that will be set when the operation is complete.
     */
    std::future<tl::expected<void, std::string>>
    update_sender_configuration(Id sender_id, RavennaSender::ConfigurationUpdate update);

    /**
     * Adds a subscriber to the node.
     * This method can be called from any thread, and will wait until the operation is complete.
     * @param subscriber The subscriber to add.
     */
    [[nodiscard]] std::future<void> subscribe(Subscriber* subscriber);

    /**
     * Removes a subscriber from the node.
     * This method can be called from any thread, and will wait until the operation is complete.
     * @param subscriber The subscriber to remove.
     */
    [[nodiscard]] std::future<void> unsubscribe(Subscriber* subscriber);

    /**
     * Adds a subscriber to the receiver with the given id.
     * @param receiver_id The id of the stream to add the subscriber to.
     * @param subscriber The subscriber to add.
     * @return A future that will be set when the operation is complete.
     */
    [[nodiscard]] std::future<void> subscribe_to_receiver(Id receiver_id, rtp::StreamReceiver::Subscriber* subscriber);

    /**
     * Removes a subscriber from the receiver with the given id.
     * @param receiver_id The id of the stream to remove the subscriber from.
     * @param subscriber The subscriber to remove.
     * @return A future that will be set when the operation is complete.
     */
    [[nodiscard]] std::future<void>
    unsubscribe_from_receiver(Id receiver_id, rtp::StreamReceiver::Subscriber* subscriber);

    /**
     * Adds a subscriber to the sender with the given id.
     * @param sender_id The id of the stream to add the subscriber to.
     * @param subscriber The subscriber to add.
     * @return A future that will be set when the operation is complete.
     */
    std::future<void> subscribe_to_sender(Id sender_id, RavennaSender::Subscriber* subscriber);

    /**
     * Removes a subscriber from the sender with the given id.
     * @param sender_id The id of the stream to remove the subscriber from.
     * @param subscriber The subscriber to remove.
     * @return A future that will be set when the operation is complete.
     */
    std::future<void> unsubscribe_from_sender(Id sender_id, RavennaSender::Subscriber* subscriber);

    /**
     * Get the packet statistics for the given stream, if the stream for the given ID exists.
     * @param receiver_id The ID of the stream to get the packet statistics for.
     * @return The packet statistics for the stream, or an empty structure if the stream doesn't exist.
     */
    [[nodiscard]] std::future<rtp::StreamReceiver::StreamStats> get_stats_for_receiver(Id receiver_id);

    /**
     * Calls back with the ravenna receiver for the given receiver id. If the stream is not found, the callback will not
     * be called and the future will be set to false.
     * @param receiver_id The id of the receiver to get the receiver for.
     * @param update_function The function to call with the receiver.
     */
    [[nodiscard]] std::future<bool> get_receiver(Id receiver_id, std::function<void(RavennaReceiver&)> update_function);

    /**
     * Get the SDP for the receiver with the given id.
     * @param receiver_id The id of the receiver to get the SDP for.
     * @return The SDP for the receiver.
     */
    [[nodiscard]] std::future<std::optional<sdp::SessionDescription>> get_sdp_for_receiver(Id receiver_id);

    /**
     * Get the SDP text for the receiver with the given id. This is the original SDP text as received from the server,
     * and might contain things which haven't been parsed into the session_description.
     * @param receiver_id The id of the receiver to get the SDP text for.
     * @return The SDP text for the receiver.
     */
    [[nodiscard]] std::future<std::optional<std::string>> get_sdp_text_for_receiver(Id receiver_id);

    /**
     * Reads the data from the receiver with the given id.
     * @param receiver_id The id of the receiver to read data from.
     * @param buffer The buffer to read the data into.
     * @param buffer_size The size of the buffer.
     * @param at_timestamp The optional timestamp to read at. If nullopt, the most recent timestamp minus the delay will
     * be used for the first read and after that the timestamp will be incremented by the packet time.
     * @return The timestamp at which the data was read, or std::nullopt if an error occurred.
     */
    [[nodiscard]] std::optional<uint32_t>
    read_data_realtime(Id receiver_id, uint8_t* buffer, size_t buffer_size, std::optional<uint32_t> at_timestamp);

    /**
     * Reads the data from the receiver with the given id.
     * @param receiver_id The id of the receiver to read data from.
     * @param output_buffer The buffer to read the data into.
     * @param at_timestamp The optional timestamp to read at. If nullopt, the most recent timestamp minus the delay will
     * be used for the first read and after that the timestamp will be incremented by the packet time.
     * @return The timestamp at which the data was read, or std::nullopt if an error occurred.
     */
    [[nodiscard]] std::optional<uint32_t> read_audio_data_realtime(
        Id receiver_id, const AudioBufferView<float>& output_buffer, std::optional<uint32_t> at_timestamp
    );

    /**
     * Schedules data to be sent onto the network.
     * @param sender_id The id of the sender.
     * @param buffer The buffer to send.
     * @param timestamp The timestamp of the data.
     */
    [[nodiscard]] bool send_data_realtime(Id sender_id, BufferView<const uint8_t> buffer, uint32_t timestamp);

    /**
     * Schedules audio data to be sent onto the network.
     * @param sender_id The id of the sender.
     * @param buffer The buffer to send.
     * @param timestamp The timestamp of the data.
     * @return True if the data was sent, false if something went wrong.
     */
    [[nodiscard]] bool send_audio_data_realtime(Id sender_id, const AudioBufferView<const float>& buffer, uint32_t timestamp);

    /**
     * @return True if this method is called on the maintenance thread, false otherwise.
     */
    [[nodiscard]] bool is_maintenance_thread() const;

    /**
     * Schedules some work on the maintenance thread using asio::dispatch. This is useful for synchronizing with
     * callbacks from the node and to offload work from the main (UI) thread. If passing using asio::use_future, the
     * future will be set when the work is complete.
     *
     * Example:
     *     node.dispatch(asio::use_future([] {
     *         return 1;
     *     })).get();
     *
     * @tparam CompletionToken The type of the completion token.
     * @param token The completion token.
     * @return The result of the dispatch operation, depending on the completion token.
     */
    template<typename CompletionToken>
    auto dispatch(CompletionToken&& token) {
        return asio::dispatch(io_context_, token);
    }

    /**
     * Schedules some work on the maintenance thread using asio::post. This is useful for synchronizing with
     * callbacks from the node and to offload work from the main (UI) thread. If passing using asio::use_future, the
     * future will be set when the work is complete.
     *
     * Example:
     *     node.post(asio::use_future([] {
     *         return 1;
     *     })).get();
     *
     * @tparam CompletionToken The type of the completion token.
     * @param token The completion token.
     * @return The result of the dispatch operation, depending on the completion token.
     */
    template<typename CompletionToken>
    auto post(CompletionToken&& token) {
        return asio::post(io_context_, token);
    }

  private:
    struct realtime_shared_context {
        std::vector<RavennaReceiver*> receivers;
        std::vector<RavennaSender*> senders;
    };

    asio::io_context io_context_;
    std::thread maintenance_thread_;
    std::thread::id maintenance_thread_id_;
    asio::ip::address_v4 interface_address_;

    RavennaBrowser browser_ {io_context_};
    RavennaRtspClient rtsp_client_ {io_context_, browser_};
    std::unique_ptr<rtp::Receiver> rtp_receiver_;
    std::vector<std::unique_ptr<RavennaReceiver>> receivers_;

    std::unique_ptr<dnssd::Advertiser> advertiser_;
    rtsp::Server rtsp_server_;
    ptp::Instance ptp_instance_;
    std::vector<std::unique_ptr<RavennaSender>> senders_;

    SubscriberList<Subscriber> subscribers_;

    RealtimeSharedObject<realtime_shared_context> realtime_shared_context_;

    [[nodiscard]] bool update_realtime_shared_context();
};

}  // namespace rav
