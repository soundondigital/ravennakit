/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "detail/rtp_buffer.hpp"
#include "detail/rtp_filter.hpp"
#include "detail/rtp_packet_stats.hpp"
#include "detail/rtp_receiver.hpp"
#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/core/util/exclusive_access_guard.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"
#include "ravennakit/core/sync/rcu.hpp"
#include "ravennakit/core/util/rank.hpp"
#include "ravennakit/core/util/safe_function.hpp"
#include "ravennakit/core/util/throttle.hpp"

#include <utility>

namespace rav::rtp {

class AudioReceiver: public Receiver::Subscriber {
  public:
    /// The number of milliseconds after which a stream is considered inactive.
    static constexpr uint64_t k_receive_timeout_ms = 1000;

    /// The length of the receiver buffer in milliseconds.
    /// AES67 specifies at least 20 ms or 20 times the packet time, whichever is smaller, but since we're on desktop
    /// systems we go a bit higher. Note that this number is not the same as the delay or added latency.
    static constexpr uint32_t k_buffer_size_ms = 200;

    /**
     * The state of a session.
     */
    enum class State {
        /// The session is idle which is expected because no parameters have been set.
        idle,
        /// The session is waiting for the first data.
        waiting_for_data,
        /// The session is running, packets are being received and consumed.
        ok,
        /// The session is running, packets are being received but not consumed.
        ok_no_consumer,
        /// The session is inactive because no packets have been received for a while.
        inactive,
    };

    /**
     * Struct to hold the info for a stream.
     */
    struct Stream {
        Session session;
        Filter filter;
        uint16_t packet_time_frames = 0;
        Rank rank;

        friend bool operator==(const Stream& lhs, const Stream& rhs) {
            return lhs.session == rhs.session && lhs.filter == rhs.filter && lhs.rank == rhs.rank
                && lhs.packet_time_frames == rhs.packet_time_frames;
        }

        friend bool operator!=(const Stream& lhs, const Stream& rhs) {
            return !(lhs == rhs);
        }
    };

    /**
     * Struct to hold the parameters of the receiver.
     */
    struct Parameters {
        AudioFormat audio_format;
        std::vector<Stream> streams;

        friend bool operator==(const Parameters& lhs, const Parameters& rhs) {
            return std::tie(lhs.audio_format, lhs.streams) == std::tie(rhs.audio_format, rhs.streams);
        }

        friend bool operator!=(const Parameters& lhs, const Parameters& rhs) {
            return !(lhs == rhs);
        }
    };

    /**
     * A struct to hold the packet and interval statistics for the stream.
     */
    struct SessionStats {
        /// The packet interval statistics.
        SlidingStats::Stats packet_interval_stats;
        /// The packet statistics.
        PacketStats::Counters packet_stats;
    };

    /**
     * Sets a callback for when data is received.
     * The timestamp will monotonically increase, but might have gaps because out-of-order and dropped packets.
     * @param callback The callback to call when data is received.
     */
    SafeFunction<void(WrappingUint32 packet_timestamp)> on_data_received_callback;

    /**
     * Sets a callback for when data is ready to be consumed.
     * The timestamp will be the timestamp of the packet which triggered this event, minus the delay. This makes it
     * convenient for consumers to read data from the buffer when the delay has passed. There will be no gaps in
     * timestamp as newer packets will trigger this event for lost packets, and out of order packet (which are
     * basically lost, not lost but late packets) will be ignored.
     * @param callback The callback to call when data is ready to be consumed.
     */
    SafeFunction<void(WrappingUint32 packet_timestamp)> on_data_ready_callback;

    /**
     * Sets a callback for when the state of the receiver changes.
     * @param callback The callback to call when the state changes.
     */
    SafeFunction<void(const Stream& stream, State state)> on_state_changed_callback;

    AudioReceiver(boost::asio::io_context& io_context, Receiver& rtp_receiver);
    ~AudioReceiver() override;

    AudioReceiver(const AudioReceiver&) = delete;
    AudioReceiver& operator=(const AudioReceiver&) = delete;

    AudioReceiver(AudioReceiver&&) noexcept = delete;
    AudioReceiver& operator=(AudioReceiver&&) noexcept = delete;

    /**
     * Sets the parameters of the receiver. This will also start the receiver if it is not already running and the
     * receiver will be restarted if necessary.
     * @param new_parameters The new parameters.
     * @return True if the parameters were changed, false if not.
     */
    bool set_parameters(Parameters new_parameters);

    /**
     * @return The current parameters of the stream.
     */
    const Parameters& get_parameters() const;

    /**
     * Reads data from the buffer at the given timestamp.
     *
     * Calling this function is realtime safe and thread safe when called from a single arbitrary thread.
     *
     * @param buffer The destination to write the data to.
     * @param buffer_size The size of the buffer in bytes.
     * @param at_timestamp The optional timestamp to read at. If nullopt, the most recent timestamp minus the delay will
     * be used for the first read and after that the timestamp will be incremented by the packet time.
     * @return The timestamp at which the data was read, or std::nullopt if an error occurred.
     */
    [[nodiscard]] std::optional<uint32_t>
    read_data_realtime(uint8_t* buffer, size_t buffer_size, std::optional<uint32_t> at_timestamp);

    /**
     * Reads the data from the receiver with the given id.
     *
     * Calling this function is realtime safe and thread safe when called from a single arbitrary thread.
     *
     * @param output_buffer The buffer to read the data into.
     * @param at_timestamp The optional timestamp to read at. If nullopt, the most recent timestamp minus the delay will
     * be used for the first read and after that the timestamp will be incremented by the packet time.
     * @return The timestamp at which the data was read, or std::nullopt if an error occurred.
     */
    [[nodiscard]] std::optional<uint32_t>
    read_audio_data_realtime(AudioBufferView<float> output_buffer, std::optional<uint32_t> at_timestamp);

    /**
     * @return The packet statistics for the first stream, if it exists, otherwise an empty structure.
     */
    [[nodiscard]] SessionStats get_session_stats(Rank rank) const;

    /**
     * @param session The session to get the state for.
     * @return The state of the session, or std::nullopt if the session is not found.
     */
    std::optional<State> get_state_for_stream(const Session& session) const;

    /**
     * Sets the delay in frames.
     * @param delay_frames The delay in frames.
     */
    void set_delay_frames(uint32_t delay_frames);

    /**
     * Sets whether the receiver is enabled or not.
     * @param enabled Whether the receiver is enabled or not.
     */
    void set_enabled(bool enabled);

    /**
     * Sets the interface address for the receiver.
     * @param interface_addresses A map of interface addresses to set. The key is the rank of the interface address.
     */
    void set_interfaces(const std::map<Rank, boost::asio::ip::address_v4>& interface_addresses);

    // Receiver::Subscriber overrides
    void on_rtp_packet(const Receiver::RtpPacketEvent& rtp_event) override;
    void on_rtcp_packet(const Receiver::RtcpPacketEvent& rtcp_event) override;

    /**
     * @return A string representation of ReceiverState.
     */
    [[nodiscard]] static const char* to_string(State state);

  private:
    /// Used for copying received packets to the realtime context.
    struct IntermediatePacket {
        uint32_t timestamp;
        uint16_t seq;
        uint16_t data_len;
        uint16_t packet_time_frames;
        std::array<uint8_t, aes67::constants::k_max_payload> data;
    };

    /**
     * Struct to hold the state and statistics for a session.
     */
    struct StreamContext {
        explicit StreamContext(Stream info);

        Stream stream_info;
        WrappingUint64 last_packet_time_ns;
        PacketStats packet_stats;
        SlidingStats packet_interval_stats {1000};
        State state {State::idle};
        // Network thread writes and audio thread reads:
        FifoBuffer<IntermediatePacket, Fifo::Spsc> fifo;
        // Audio thread writes and network thread reads:
        FifoBuffer<uint16_t, Fifo::Spsc> packets_too_old;
    };

    struct SharedContext {
        // Audio thread:
        Buffer receiver_buffer;
        std::vector<uint8_t> read_buffer;
        std::optional<WrappingUint32> first_packet_timestamp;
        WrappingUint32 next_ts;
        AudioFormat selected_audio_format;

        // Read by audio and network thread:
        uint32_t delay_frames = 0;
        std::vector<StreamContext*> stream_contexts;
    };

    Receiver& rtp_receiver_;
    boost::asio::steady_timer maintenance_timer_;
    ExclusiveAccessGuard realtime_access_guard_;

    Parameters parameters_;
    uint32_t delay_frames_ {};
    bool enabled_ {};

    std::map<Rank, boost::asio::ip::address_v4> interface_addresses_;
    std::vector<std::unique_ptr<StreamContext>> stream_contexts_;

    bool is_running_ {false};
    std::optional<WrappingUint32> rtp_ts_;
    WrappingUint16 seq_;
    Throttle<void> packet_interval_throttle_ {std::chrono::seconds(10)};
    Throttle<PacketStats::Counters> packet_stats_throttle_ {std::chrono::seconds(5)};
    // Read and write by both threads. Whether data is being consumed. When the FIFO is full, this will be set to false.
    std::atomic_bool consumer_active_ {false};

    Rcu<SharedContext> shared_context_;
    Rcu<SharedContext>::Reader audio_thread_reader_ {shared_context_.create_reader()};
    Rcu<SharedContext>::Reader network_thread_reader_ {shared_context_.create_reader()};

    void update_shared_context();
    void do_maintenance();
    void do_realtime_maintenance();
    void set_state(StreamContext& stream_context, State new_state) const;
    void start();
    void stop();
    StreamContext* find_stream_context(const Session& session);
    const StreamContext* find_stream_context(const Session& session) const;
};

}  // namespace rav::rtp
