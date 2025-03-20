/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/ptp_port.hpp"

#include "ravennakit/core/random.hpp"
#include "ravennakit/core/util.hpp"
#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/ptp/ptp_constants.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ptp/ptp_profiles.hpp"
#include "ravennakit/ptp/messages/ptp_delay_resp_message.hpp"
#include "ravennakit/ptp/messages/ptp_follow_up_message.hpp"
#include "ravennakit/ptp/messages/ptp_pdelay_resp_follow_up_message.hpp"
#include "ravennakit/ptp/messages/ptp_pdelay_resp_message.hpp"

#include <random>

namespace {
const auto k_ptp_multicast_address = asio::ip::make_address("224.0.1.129");
constexpr auto k_ptp_event_port = 319;
constexpr auto k_ptp_general_port = 320;
}  // namespace

rav::ptp::Port::Port(
    Instance& parent, asio::io_context& io_context, const asio::ip::address& interface_address,
    const PortIdentity port_identity
) :
    parent_(parent),
    announce_receipt_timeout_timer_(io_context),
    event_socket_(io_context, asio::ip::address_v4(), k_ptp_event_port),
    general_socket_(io_context, asio::ip::address_v4(), k_ptp_general_port) {
    // Initialize the port data set
    port_ds_.port_identity = port_identity;
    port_ds_.delay_mechanism = DelayMechanism::e2e;  // TODO: Make this configurable
    set_state(State::initializing);

    subscriptions_.push_back(event_socket_.join_multicast_group(k_ptp_multicast_address, interface_address));
    subscriptions_.push_back(general_socket_.join_multicast_group(k_ptp_multicast_address, interface_address));

    if (const auto ec = event_socket_.set_multicast_loopback(false)) {
        RAV_WARNING("Failed to set multicast loopback for event socket: {}", ec.message());
    }
    if (const auto ec = general_socket_.set_multicast_loopback(false)) {
        RAV_WARNING("Failed to set multicast loopback for general socket: {}", ec.message());
    }

    if (interface_address.is_v4()) {
        if (const auto ec = event_socket_.set_multicast_outbound_interface(interface_address.to_v4())) {
            RAV_ERROR("Failed to set multicast outbound interface for event socket: {}", ec.message());
        }
        if (const auto ec = general_socket_.set_multicast_outbound_interface(interface_address.to_v4())) {
            RAV_ERROR("Failed to set multicast outbound interface for general socket: {}", ec.message());
        }
    } else {
        RAV_WARNING("Interface address is not IPv4. Cannot set multicast outbound interface.");
    }

    event_socket_.set_dscp_value(46);    // Default AES67 value
    general_socket_.set_dscp_value(46);  // Default AES67 value

    auto handler = [this](const rtp::UdpSenderReceiver::recv_event& event) {
        handle_recv_event(event);
    };

    event_socket_.start(handler);
    general_socket_.start(handler);

    set_state(State::listening);

    schedule_announce_receipt_timeout();
}

rav::ptp::Port::~Port() = default;

const rav::ptp::PortIdentity& rav::ptp::Port::get_port_identity() const {
    return port_ds_.port_identity;
}

void rav::ptp::Port::assert_valid_state(const Profile& profile) const {
    port_ds_.assert_valid_state(profile);
}

void rav::ptp::Port::apply_state_decision_algorithm(
    const DefaultDs& default_ds, const std::optional<BestAnnounceMessage>& ebest
) {
    if (!ebest && port_ds_.port_state == State::listening) {
        return;
    }

    auto recommended_state = calculate_recommended_state(default_ds, ebest->get_comparison_data_set());
    if (!recommended_state) {
        RAV_TRACE("Port is listening, and no ebest is available. No state change is recommended.");
        return;
    }

    // IEEE 1588-2019: Figure 31 states that in a slave-only configuration, when BMC_MASTER or BMC_PASSIVE is
    // recommended the port should go into listening state. However, when this instance is also the best master
    // according to the state decision algorithm, it will never go into slave state. Since the result is a
    // recommendation anyway, I'm taking the liberty to place the PTP instance into slave state instead of listening
    // state.
    if (default_ds.slave_only) {
        recommended_state = StateDecisionCode::s1;
    }

    parent_.set_recommended_state(recommended_state.value(), ebest ? std::optional(ebest->message) : std::nullopt);
    set_state(parent_.get_state_for_decision_code(*recommended_state));
}

std::optional<rav::ptp::StateDecisionCode> rav::ptp::Port::calculate_recommended_state(
    const DefaultDs& default_ds, const std::optional<ComparisonDataSet>& ebest
) const {
    if (!ebest && port_ds_.port_state == State::listening) {
        return std::nullopt;
    }

    const ComparisonDataSet d0(default_ds);

    if (range(1, 127).contains(default_ds.clock_quality.clock_class)) {
        // D0 better or better by topology than Erbest
        if (!erbest_
            || d0.compare(ComparisonDataSet(*erbest_, port_ds_.port_identity))
                >= ComparisonDataSet::result::better_by_topology) {
            return StateDecisionCode::m1;  // BMC_MASTER (D0)
        }
        return StateDecisionCode::p1;  // BMC_PASSIVE (Erbest)
    }

    // D0 better or better by topology than Ebest
    if (!ebest || d0.compare(*ebest) >= ComparisonDataSet::result::better_by_topology) {
        return StateDecisionCode::m2;  // BMC_MASTER (D0)
    }

    // Ebest received on port R
    if (ebest->identity_of_receiver == port_ds_.port_identity) {
        return StateDecisionCode::s1;  // BMC_SLAVE (Ebest = Erbest)
    }

    // Ebest better by topology than Erbest
    if (!erbest_
        || ebest->compare(ComparisonDataSet(*erbest_, port_ds_.port_identity))
            == ComparisonDataSet::result::better_by_topology) {
        return StateDecisionCode::p2;  // BMC_PASSIVE (Erbest)
    }

    return StateDecisionCode::m3;  // BMC_MASTER (Ebest)
}

void rav::ptp::Port::schedule_announce_receipt_timeout() {
    const auto random_factor = random().get_random_int(0, 1000) / 1000.0;
    const auto announce_interval_ms = static_cast<int>(std::pow(2, port_ds_.log_announce_interval) * 1000);
    const auto announce_receipt_timeout = port_ds_.announce_receipt_timeout * announce_interval_ms
        + static_cast<int>(random_factor * announce_interval_ms);

    announce_receipt_timeout_timer_.expires_after(std::chrono::milliseconds(announce_receipt_timeout));
    announce_receipt_timeout_timer_.async_wait([this](const std::error_code& error) {
        if (error == asio::error::operation_aborted) {
            return;
        }
        if (error) {
            RAV_ERROR("Announce receipt timeout timer error: {}", error.message());
        }
        trigger_announce_receipt_timeout_expires_event();
        schedule_announce_receipt_timeout();
    });
}

void rav::ptp::Port::set_state(const State new_state) {
    if (new_state == port_ds_.port_state) {
        return;
    }

    // IEEE 1588-2019: 9.2.6.12.c Update time state
    switch (new_state) {
        case State::listening:
        case State::passive:
        case State::uncalibrated:
        case State::slave:
            schedule_announce_receipt_timeout();
            break;
        case State::master:
        case State::pre_master:
            RAV_ASSERT_FALSE("Master state not implemented");
            return;
        case State::initializing:
        case State::faulty:
        case State::disabled:
            announce_receipt_timeout_timer_.cancel();
            break;
        case State::undefined:
            break;
    }

    port_ds_.port_state = new_state;

    RAV_INFO("Switching port {} to {}", port_ds_.port_identity.port_number, to_string(new_state));

    parent_.on_port_changed_state({*this});
}

rav::ptp::Measurement<double> rav::ptp::Port::calculate_offset_from_master(const SyncMessage& sync_message) const {
    RAV_ASSERT(!sync_message.header.flags.two_step_flag, "Use the other method for two-step sync messages");
    const auto corrected_sync_correction_field =
        TimeInterval::from_wire_format(sync_message.header.correction_field)
            .total_seconds_double();  // TODO: Ignoring delay asymmetry for now
    const auto t1 = sync_message.origin_timestamp.total_seconds_double();
    const auto t2 = sync_message.receive_timestamp.total_seconds_double();
    // Note: using mean_delay_stats_.median() with a window of 32 instead of mean_delay_ worked also quite well
    const auto offset = t2 - t1 - mean_delay_ - corrected_sync_correction_field;
    return {t2, offset, mean_delay_, {}};
}

rav::ptp::Measurement<double> rav::ptp::Port::calculate_offset_from_master(
    const SyncMessage& sync_message, const FollowUpMessage& follow_up_message
) const {
    RAV_ASSERT(sync_message.header.flags.two_step_flag, "Use the other method for one-step sync messages");
    const auto corrected_sync_correction_field =
        TimeInterval::from_wire_format(sync_message.header.correction_field)
            .total_seconds_double();  // TODO: Ignoring delay asymmetry for now
    const auto follow_up_correction_field =
        TimeInterval::from_wire_format(follow_up_message.header.correction_field).total_seconds_double();
    const auto t1 = follow_up_message.precise_origin_timestamp.total_seconds_double();
    const auto t2 = sync_message.receive_timestamp.total_seconds_double();
    // Note: using mean_delay_stats_.median() with a window of 32 instead of mean_delay_ worked also quite well
    const auto offset = t2 - t1 - mean_delay_ - corrected_sync_correction_field - follow_up_correction_field;
    return {t2, offset, mean_delay_, {}};
}

void rav::ptp::Port::trigger_announce_receipt_timeout_expires_event() {
    TRACY_ZONE_SCOPED;

    erbest_.reset();
    if (parent_.default_ds().slave_only) {
        set_state(State::listening);
    } else {
        RAV_ASSERT_FALSE("Master state not implemented");
    }
}

void rav::ptp::Port::process_request_response_delay_sequence() {
    TRACY_ZONE_SCOPED;

    if (!(port_ds_.port_state == State::slave || port_ds_.port_state == State::uncalibrated)) {
        RAV_WARNING("Request-response delay sequence should only be processed in slave or uncalibrated state");
    }
    if (port_ds_.delay_mechanism != DelayMechanism::e2e) {
        RAV_WARNING("Request-response delay sequence should only be processed in E2E delay mechanism");
    }

    const auto now = parent_.get_local_ptp_time();
    for (auto& seq : request_response_delay_sequences_) {
        if (seq.get_state() == RequestResponseDelaySequence::state::ready_to_be_scheduled) {
            seq.schedule_delay_req_message_send(port_ds_);
        }
        if (auto send_after = seq.get_delay_req_scheduled_send_time()) {
            if (now >= send_after.value()) {
                send_delay_req_message(seq);
            }
        }
    }
}

void rav::ptp::Port::send_delay_req_message(RequestResponseDelaySequence& sequence) {
    TRACY_ZONE_SCOPED;

    const auto msg = sequence.create_delay_req_message(port_ds_);
    send_buffer_.clear();
    msg.write_to(send_buffer_);
    tracy_point();
    event_socket_.send(send_buffer_.data(), send_buffer_.size(), {k_ptp_multicast_address, k_ptp_event_port});
    tracy_point();
    sequence.set_delay_req_sent_time(parent_.get_local_ptp_time());
}

rav::ptp::State rav::ptp::Port::state() const {
    return port_ds_.port_state;
}

std::optional<rav::ptp::BestAnnounceMessage>
rav::ptp::Port::determine_ebest(const std::vector<std::unique_ptr<Port>>& ports) {
    const Port* best_port = nullptr;

    for (auto& port : ports) {
        if (port == nullptr) {
            RAV_ASSERT_FALSE("Found a nullptr in the port list");
            continue;
        }

        port->calculate_erbest();

        if (!port->erbest_) {
            continue;
        }

        if (best_port == nullptr) {
            best_port = port.get();
            continue;
        }

        if (!best_port->erbest_) {
            best_port = port.get();
            continue;
        }

        ComparisonDataSet best_port_set(best_port->erbest_.value(), best_port->port_ds_.port_identity);
        ComparisonDataSet port_set(port->erbest_.value(), port->port_ds_.port_identity);

        const auto r = port_set.compare(best_port_set);

        if (r >= ComparisonDataSet::result::error1 && r <= ComparisonDataSet::result::error2) {
            RAV_ERROR("PTP data set comparison failed");
            return std::nullopt;
        }

        if (r >= ComparisonDataSet::result::better_by_topology) {
            best_port = port.get();
        }
    }

    if (best_port == nullptr) {
        return std::nullopt;
    }

    RAV_ASSERT(best_port->erbest_, "Best port should have a valid Erbest");

    return BestAnnounceMessage {*best_port->erbest_, best_port->port_ds_.port_identity};
}

const rav::ptp::PortDs& rav::ptp::Port::port_ds() const {
    return port_ds_;
}

void rav::ptp::Port::increase_age() {
    foreign_master_list_.increase_age();
}

void rav::ptp::Port::handle_recv_event(const rtp::UdpSenderReceiver::recv_event& event) {
    TRACY_ZONE_SCOPED;

    const buffer_view data(event.data, event.size);
    auto header = MessageHeader::from_data(data);
    if (!header) {
        RAV_TRACE("PTP Header error: {}", to_string(header.error()));
        return;
    }

    if (!parent_.should_process_ptp_messages(header.value())) {
        return;
    }

    // RAV_TRACE(
    //     "{} from {}", to_string(header.value().message_type),
    //     header.value().source_port_identity.clock_identity.to_string()
    // );

    if (port_ds_.port_state == State::initializing) {
        RAV_TRACE("Discarding announce message while initializing");
        return;
    }

    if (port_ds_.port_state == State::disabled) {
        RAV_TRACE("Discarding announce message while disabled");
        return;
    }

    if (port_ds_.port_state == State::faulty) {
        RAV_TRACE("Discarding announce message while faulty");
        return;
    }

    switch (header->message_type) {
        case MessageType::announce: {
            auto announce_message =
                AnnounceMessage::from_data(header.value(), data.subview(MessageHeader::k_header_size));
            if (!announce_message) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(announce_message.error()));
            }
            handle_announce_message(announce_message.value(), {});
            break;
        }
        case MessageType::sync: {
            auto sync_message =
                SyncMessage::from_data(header.value(), data.subview(MessageHeader::k_header_size));
            if (!sync_message) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(sync_message.error()));
            }
            handle_sync_message(sync_message.value(), {});
            break;
        }
        case MessageType::delay_req:
        case MessageType::p_delay_req: {
            // Ignoring delay request messages because master functionality is not implemented
            break;
        }
        case MessageType::p_delay_resp: {
            auto pdelay_resp = PdelayRespMessage::from_data(data.subview(MessageHeader::k_header_size));
            if (!pdelay_resp) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(pdelay_resp.error()));
            }
            handle_pdelay_resp_message(pdelay_resp.value(), {});
            break;
        }
        case MessageType::follow_up: {
            auto follow_up =
                FollowUpMessage::from_data(header.value(), data.subview(MessageHeader::k_header_size));
            if (!follow_up) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(follow_up.error()));
            }
            handle_follow_up_message(follow_up.value(), {});
            break;
        }
        case MessageType::delay_resp: {
            auto delay_resp =
                DelayRespMessage::from_data(header.value(), data.subview(MessageHeader::k_header_size));
            if (!delay_resp) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(delay_resp.error()));
            }
            handle_delay_resp_message(delay_resp.value(), {});
            break;
        }
        case MessageType::p_delay_resp_follow_up: {
            auto pdelay_resp_follow_up =
                PdelayRespFollowUpMessage::from_data(data.subview(MessageHeader::k_header_size));
            if (!pdelay_resp_follow_up) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(pdelay_resp_follow_up.error()));
            }
            handle_pdelay_resp_follow_up_message(pdelay_resp_follow_up.value(), {});
            break;
        }
        case MessageType::signaling: {
            RAV_ASSERT_FALSE("Signaling messages are not implemented");
            break;
        }
        case MessageType::management: {
            RAV_ASSERT_FALSE("Management messages are not implemented");
            break;
        }
        case MessageType::reserved1:
        case MessageType::reserved2:
        case MessageType::reserved3:
        case MessageType::reserved4:
        case MessageType::reserved5:
        case MessageType::reserved6:
        default: {
            RAV_WARNING("Unknown PTP message received: {}", to_string(header->message_type));
        }
    }
}

void rav::ptp::Port::handle_announce_message(
    const AnnounceMessage& announce_message, buffer_view<const uint8_t> tlvs
) {
    TRACY_ZONE_SCOPED;

    std::ignore = announce_message;
    std::ignore = tlvs;

    // IEEE 1588-2019: 9.3.2.5.a If a message comes from the same PTP instance, the message is not qualified.
    if (announce_message.header.source_port_identity.clock_identity == port_ds_.port_identity.clock_identity) {
        RAV_WARNING("Message is not qualified because it comes from the same PTP instance");
        return;
    }

    // IEEE 1588-2019: 9.3.2.5.d If steps removed of 255 or greater, message is not qualified.
    if (announce_message.steps_removed >= 255) {
        RAV_WARNING("Message is not qualified because steps removed is 255 or greater");
        return;
    }

    // IEEE 1588-2019: 9.5.3
    if (port_ds_.port_state == State::slave || port_ds_.port_state == State::uncalibrated) {
        if (announce_message.header.source_port_identity == parent_.get_parent_ds().parent_port_identity) {
            parent_.set_recommended_state(StateDecisionCode::s1, announce_message);
            schedule_announce_receipt_timeout();
        } else {
            // Message is considered foreign
            foreign_master_list_.add_or_update_entry(announce_message);
        }
    } else {
        // Message is considered foreign
        foreign_master_list_.add_or_update_entry(announce_message);
    }

    // IEEE 1588-2019: 9.3.2.3.c If the port state is Slave, Uncalibrated, or Passive, the previous Erbest is used, and
    // updated with newer messages from this port.
    if (port_ds_.port_state == State::slave || port_ds_.port_state == State::uncalibrated
        || port_ds_.port_state == State::passive) {
        if (erbest_ && erbest_->header.source_port_identity == announce_message.header.source_port_identity) {
            if (announce_message.header.sequence_id > erbest_->header.sequence_id) {
                erbest_ = announce_message;
            } else {
                RAV_WARNING("Received an older announce message from the same port");
            }
        }
    }

    // IEEE 1588-2019: 9.5.3 NOTE 2 Trigger state decision event right away
    parent_.execute_state_decision_event();
}

void rav::ptp::Port::handle_sync_message(SyncMessage sync_message, buffer_view<const uint8_t> tlvs) {
    TRACY_ZONE_SCOPED;

    std::ignore = tlvs;

    sync_message.receive_timestamp = parent_.get_local_ptp_time();

    // Ignore sync messages when not in slave or uncalibrated state
    if (!(port_ds_.port_state == State::slave || port_ds_.port_state == State::uncalibrated)) {
        return;
    }

    // Ignore follow-up messages which are not from current parent
    if (sync_message.header.source_port_identity != parent_.get_parent_ds().parent_port_identity) {
        return;
    }

    if (port_ds_.delay_mechanism == DelayMechanism::e2e) {
        if (syncs_until_delay_req_ <= 0) {
            request_response_delay_sequences_.push_back(RequestResponseDelaySequence {sync_message});
            syncs_until_delay_req_ = static_cast<int32_t>(
                std::pow(2, port_ds_.log_min_delay_req_interval) / std::pow(2, sync_message.header.log_message_interval)
            );
        } else {
            syncs_until_delay_req_--;
        }
    } else if (port_ds_.delay_mechanism == DelayMechanism::e2e) {
        TODO("Implement P2P delay mechanism");
    } else {
        RAV_ASSERT_FALSE("Unknown delay mechanism");
    }

    if (sync_message.header.flags.two_step_flag) {
        sync_messages_.push_back(sync_message);  // Wait for a follow-up message
    } else {
        parent_.update_local_ptp_clock(calculate_offset_from_master(sync_message));
    }

    process_request_response_delay_sequence();
}

void rav::ptp::Port::handle_follow_up_message(
    const FollowUpMessage& follow_up_message, buffer_view<const uint8_t> tlvs
) {
    TRACY_ZONE_SCOPED;

    std::ignore = follow_up_message;
    std::ignore = tlvs;

    // Ignore follow-up messages when not in slave or uncalibrated state
    if (!(port_ds_.port_state == State::slave || port_ds_.port_state == State::uncalibrated)) {
        return;
    }

    // Ignore follow-up messages which are not from current parent
    if (follow_up_message.header.source_port_identity != parent_.get_parent_ds().parent_port_identity) {
        return;
    }

    // Update request-response delay sequence with follow-up message if necessary
    for (auto& seq : request_response_delay_sequences_) {
        if (seq.matches(follow_up_message.header)) {
            seq.update(follow_up_message);
        }
    }

    // Find the previous sync message and update the clock
    for (auto& sync : sync_messages_) {
        if (sync.header.matches(follow_up_message.header)) {
            parent_.update_local_ptp_clock(calculate_offset_from_master(sync, follow_up_message));
            process_request_response_delay_sequence();
            return;
        }
    }

    process_request_response_delay_sequence();
    RAV_WARNING("Received follow-up message without matching sync message");
}

void rav::ptp::Port::handle_delay_resp_message(
    const DelayRespMessage& delay_resp_message, buffer_view<const uint8_t> tlvs
) {
    TRACY_ZONE_SCOPED;

    std::ignore = tlvs;

    // Ignore follow-up messages when not in slave or uncalibrated state
    if (!(port_ds_.port_state == State::slave || port_ds_.port_state == State::uncalibrated)) {
        return;
    }

    // Ignore messages which are not from current parent
    // Note: Figure 40 in section 9.5.7 of IEEE 1588-2019 suggests that the association should be tested before testing
    // whether the message is from the current parent, but based on later text the order doesn't seem to matter that
    // much, so we'll do the test here for clarity purposes.
    if (delay_resp_message.header.source_port_identity != parent_.get_parent_ds().parent_port_identity) {
        return;
    }

    if (delay_resp_message.requesting_port_identity != port_ds_.port_identity) {
        return;  // This message is not for us
    }

    for (auto& seq : request_response_delay_sequences_) {
        if (delay_resp_message.header.sequence_id == seq.get_sequence_id()) {
            port_ds_.log_min_delay_req_interval = delay_resp_message.header.log_message_interval;
            // Message is associated with earlier delay request message
            // Note: section 9.5.7 of IEEE 1588-2019 suggests that the Delay_Resp message should have a
            // requestingSequenceId field, but 13.8.1 doesn't specify this field. We'll assume that the sequence ID
            // in the header is the one to be used.
            seq.update(delay_resp_message);
            const auto mean_delay = seq.calculate_mean_path_delay();
            TRACY_PLOT("Mean delay (ms)", mean_delay * 1000.0);

            mean_delay_stats_.add(mean_delay_);
            TRACY_PLOT("Mean delay median (ms)", mean_delay_stats_.median() * 1000.0);

            if (mean_delay_stats_.count() > 10 && mean_delay_stats_.is_outlier_median(mean_delay, 0.001)) {
                TRACY_PLOT("Mean delay outliers", mean_delay * 1000.0);
                TRACY_MESSAGE("Ignoring outlier mean delay");
                RAV_WARNING("Ignoring outlier mean delay: {}", mean_delay * 1000.0);
                return;
            }
            TRACY_PLOT("Mean delay outliers", 0.0);

            mean_delay_ = mean_delay_filter_.update(mean_delay);
            TRACY_PLOT("Mean delay filtered (ms)", mean_delay_ * 1000.0);
            return;  // Done here.
        }
    }

    RAV_WARNING("Received a delay response message without matching delay request message");
}

void rav::ptp::Port::handle_pdelay_resp_message(
    const PdelayRespMessage& delay_req_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = delay_req_message;
    std::ignore = tlvs;
}

void rav::ptp::Port::handle_pdelay_resp_follow_up_message(
    const PdelayRespFollowUpMessage& delay_req_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = delay_req_message;
    std::ignore = tlvs;
}

void rav::ptp::Port::calculate_erbest() {
    if (erbest_) {
        RAV_ASSERT(
            foreign_master_list_.size() > 0,
            "Erbest is set, but there are no entries in the foreign master list. Only the entries which didn't become the best announce message should have been removed."
        );

        if (foreign_master_list_.size() == 1) {
            RAV_ASSERT(
                foreign_master_list_.begin()->foreign_master_port_identity == erbest_->header.source_port_identity,
                "Erbest is set, but the foreign master list contains an entry from a different source."
            );
        }
    }

    // IEEE 1588-2019: 9.3.2.3 Calculate Erbest
    std::optional<AnnounceMessage> erbest;

    bool should_include_previous_erbest = erbest_
        && (port_ds_.port_state == State::slave || port_ds_.port_state == State::uncalibrated
            || port_ds_.port_state == State::passive);

    for (const auto& entry : foreign_master_list_) {
        if (const auto& announce_message = entry.most_recent_announce_message) {
            if (should_include_previous_erbest
                && announce_message->header.source_port_identity == erbest_->header.source_port_identity) {
                // Don't include this entry, because it is the previous Erbest, which might have been updated outside
                // the foreign master list and which is considered later.
                continue;
            }
            // IEEE 1588-2019: 9.3.2.5.c If fewer than k_foreign_master_threshold messages have been received from the
            // foreign master within the most recent k_foreign_master_time_window, message is not qualified.
            if (entry.foreign_master_announce_messages < k_foreign_master_threshold) {
                continue;
            }
            if (erbest) {
                if (ComparisonDataSet::compare(*announce_message, *erbest, port_ds_.port_identity)
                    >= ComparisonDataSet::result::better_by_topology) {
                    erbest = announce_message;
                }
            } else {
                erbest = announce_message;
            }
        }
    }

    if (erbest) {
        if (should_include_previous_erbest) {
            if (ComparisonDataSet::compare(*erbest, *erbest_, port_ds_.port_identity)
                >= ComparisonDataSet::result::better_by_topology) {
                erbest_ = erbest;
            }
        } else {
            erbest_ = erbest;  // The non-empty set is considered better than the empty set.
        }
    }

    // IEEE 1588-2019: 9.3.2.3.b Remove entries from the foreign master list which were considered but which didn't
    // become Erbest.
    foreign_master_list_.purge_entries(erbest_);
}
