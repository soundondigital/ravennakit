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
#include "ravennakit/core/events.hpp"
#include "ravennakit/core/linked_node.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"
#include "ravennakit/rtsp/rtsp_client.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

/**
 * Maintains connections to one or more RAVENNA RTSP servers, upon request.
 */
class RavennaRtspClient: public RavennaBrowser::Subscriber {
  public:
    struct AnnouncedEvent {
        const std::string& session_name;
        const sdp::SessionDescription& sdp;
    };

    class Subscriber {
      public:
        Subscriber() = default;
        virtual ~Subscriber() = default;

        Subscriber(const Subscriber&) = delete;
        Subscriber& operator=(const Subscriber&) = delete;

        Subscriber(Subscriber&&) noexcept = delete;
        Subscriber& operator=(Subscriber&&) noexcept = delete;

        /**
         * Called when a session is announced.
         * @param event The announced event.
         */
        virtual void on_announced([[maybe_unused]] const AnnouncedEvent& event) {}
    };

    explicit RavennaRtspClient(asio::io_context& io_context, RavennaBrowser& browser);
    ~RavennaRtspClient() override;

    /**
     * Subscribes to a session.
     * @param subscriber_to_add The subscriber to add.
     * @param session_name The name of the session to subscribe to.
     * @return true if the subscriber was added, or false if it was already in the list.
     */
    [[nodiscard]] bool subscribe_to_session(Subscriber* subscriber_to_add, const std::string& session_name);

    /**
     * Unsubscribes from all sessions.
     * @param subscriber_to_remove The subscriber to remove.
     * @return true if the subscriber was removed from at least one session, or false if it wasn't.
     */
    [[nodiscard]] bool unsubscribe_from_all_sessions(Subscriber* subscriber_to_remove);

    /**
     * Tries to find the SDP for the given session.
     * @param session_name The name of the session to get the SDP for.
     * @return The SDP for the session, if it exists, otherwise an empty optional.
     */
    [[nodiscard]] std::optional<sdp::SessionDescription> get_sdp_for_session(const std::string& session_name) const;

    /**
     * Tries to find the SDP text for the given session. The difference between this and get_sdp_for_session is that the
     * return value will contain the original SDP text, including things which might not be parsed into the
     * session_description.
     * @param session_name The name of the session to get the SDP text for.
     * @return The SDP text for the session, if it exists, otherwise an empty optional.
     */
    [[nodiscard]] std::optional<std::string> get_sdp_text_for_session(const std::string& session_name) const;

    /**
     * @return The io_context used by this client.
     */
    [[nodiscard]] asio::io_context& get_io_context() const;

    // ravenna_browser::subscriber overrides
    void ravenna_session_discovered(const dnssd::Browser::ServiceResolved& event) override;

  private:
    struct SessionContext {
        std::string session_name;
        subscriber_list<Subscriber> subscribers;
        std::optional<sdp::SessionDescription> sdp_;
        std::optional<std::string> sdp_text_;
        std::string host_target;
        uint16_t port {};
    };

    struct ConnectionContext {
        std::string host_target;
        uint16_t port {};
        rtsp::Client client;
    };

    asio::io_context& io_context_;
    RavennaBrowser& browser_;
    std::vector<SessionContext> sessions_;
    std::vector<ConnectionContext> connections_;

    ConnectionContext& find_or_create_connection(const std::string& host_target, uint16_t port);
    ConnectionContext* find_connection(const std::string& host_target, uint16_t port);
    void update_session_with_service(SessionContext& session, const dnssd::ServiceDescription& service);
    void do_maintenance();
    void handle_incoming_sdp(const std::string& sdp_text);
};

}  // namespace rav
