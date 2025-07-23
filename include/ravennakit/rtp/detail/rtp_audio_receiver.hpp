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

#include "rtp_filter.hpp"
#include "rtp_packet_stats.hpp"
#include "rtp_ringbuffer.hpp"
#include "rtp_session.hpp"
#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"
#include "ravennakit/core/net/asio/asio_helpers.hpp"
#include "ravennakit/core/sync/atomic_rw_lock.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/core/util/safe_function.hpp"

#include <boost/asio.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/lockfree/spsc_value.hpp>

namespace rav::rtp {

struct AudioReceiver {
    /// The maximum number of readers.
    static constexpr auto k_max_num_readers = 16;

    /// The maximum number of redundant sessions per reader (redundant paths).
    static constexpr auto k_max_num_redundant_sessions = 2;  // How many redundant paths

    /// The number of milliseconds after which a stream is considered inactive.
    static constexpr uint64_t k_receive_timeout_ms = 1000;

    /// The length of the receiver buffer in milliseconds.
    /// AES67 specifies at least 20 ms or 20 times the packet time, whichever is smaller, but since we're on desktop
    /// systems we go a bit higher. Note that this number is not the same as the delay or added latency.
    static constexpr uint32_t k_buffer_size_ms = 200;

    using ArrayOfAddresses = std::array<ip_address_v4, k_max_num_redundant_sessions>;

    struct StreamInfo {
        Session session;
        Filter filter;
        uint16_t packet_time_frames {};

        [[nodiscard]] auto tie() const {
            return std::tie(session, filter, packet_time_frames);
        }

        friend bool operator==(const StreamInfo& lhs, const StreamInfo& rhs) {
            return lhs.tie() == rhs.tie();
        }

        friend bool operator!=(const StreamInfo& lhs, const StreamInfo& rhs) {
            return lhs.tie() != rhs.tie();
        }

        [[nodiscard]] bool is_valid() const {
            return session.valid() && packet_time_frames > 0;
        }
    };

    struct ReaderParameters {
        AudioFormat audio_format;
        std::array<StreamInfo, k_max_num_redundant_sessions> streams;

        [[nodiscard]] auto tie() const {
            return std::tie(audio_format, streams);
        }

        friend bool operator==(const ReaderParameters& lhs, const ReaderParameters& rhs) {
            return lhs.tie() == rhs.tie();
        }

        friend bool operator!=(const ReaderParameters& lhs, const ReaderParameters& rhs) {
            return lhs.tie() != rhs.tie();
        }

        [[nodiscard]] bool is_valid() const {
            if (!audio_format.is_valid()) {
                return false;
            }
            for (auto& stream : streams) {
                if (stream.is_valid()) {
                    return true;  // At least one stream needs to be valid.
                }
            }
            return false;
        }
    };

    /**
     * The state of a reader.
     */
    enum class StreamState {
        /// The stream is inactive because no packets have been received for a while.
        inactive,
        /// Packets are being received and consumed.
        receiving,
        /// Packets are being received, but they are not consumed.
        no_consumer,
    };

    explicit AudioReceiver(boost::asio::io_context& io_context);
    ~AudioReceiver();

    /**
     * Adds a reader to the receiver.
     * Thread safe: no.
     * @param id The id to use, must be unique.
     * @param parameters The parameters of a reader.
     * @param interfaces The interfaces to receive multicast sessions on.
     * @return true if a new reader was added, or false if not.
     */
    [[nodiscard]] bool add_reader(Id id, const ReaderParameters& parameters, const ArrayOfAddresses& interfaces);

    /**
     * Removes the reader with given id, if it exists.
     * Thread safe: no.
     * @param id The id of the reader to remove.
     * @return true if a reader was removed, or false if not.
     */
    [[nodiscard]] bool remove_reader(Id id);

    /**
     * Sets the interfaces on all readers, leaving and joining multicast groups where necessary.
     * @param interfaces The new interfaces to use.
     */
    void set_interfaces(const ArrayOfAddresses& interfaces);

    /**
     * Call this to read incoming packets and place the data inside a fifo for consumption. Should be called from a
     * single high priority thread with regular short intervals.
     */
    void read_incoming_packets();

    /**
     * Reads data from the buffer at the given timestamp.
     *
     * Calling this function is realtime safe and thread safe when called from a single arbitrary thread.
     *
     * @param id The id of the reader to get data from.
     * @param buffer The destination to write the data to.
     * @param buffer_size The size of the buffer in bytes.
     * @param at_timestamp The optional timestamp to read at. If nullopt, the most recent timestamp minus the delay will
     * be used for the first read and after that the timestamp will be incremented by the packet time.
     * @param require_delay If set, the call to read_data_realtime will only succeed if the requested timestamp is
     * older than the most recent received timestamp - require_delay. This can be useful in a case where there is no PTP
     * clock driving time, and instead, the time of the RTP stream has to be used. In normal PTP driven operation you
     * would not set require_delay.
     * @return The timestamp at which the data was read, or std::nullopt if an error occurred.
     */
    [[nodiscard]] std::optional<uint32_t> read_data_realtime(
        Id id, uint8_t* buffer, size_t buffer_size, std::optional<uint32_t> at_timestamp,
        std::optional<uint32_t> require_delay
    );

    /**
     * Reads the data from the receiver with the given id.
     *
     * Calling this function is realtime safe and thread safe when called from a single arbitrary thread.
     *
     * @param id The id of the reader to get data from.
     * @param output_buffer The buffer to read the data into.
     * @param at_timestamp The optional timestamp to read at. If nullopt, the most recent timestamp minus the delay will
     * be used for the first read and after that the timestamp will be incremented by the packet time.
     * @param require_delay If set, the call to read_audio_data_realtime will only succeed if the requested timestamp is
     * older than the most recent received timestamp - require_delay. This can be useful in a case where there is no PTP
     * clock driving time, and instead, the time of the RTP stream has to be used. In normal PTP driven operation you
     * would not set require_delay.
     * @return The timestamp at which the data was read, or std::nullopt if an error occurred.
     */
    [[nodiscard]] std::optional<uint32_t> read_audio_data_realtime(
        Id id, AudioBufferView<float> output_buffer, std::optional<uint32_t> at_timestamp,
        std::optional<uint32_t> require_delay
    );

    /**
     * @param reader_id The id of the reader to get statistics from.
     * @param stream_index The index of the stream to get stats from.
     * @return The statistics for given stream index, or nullopt if no statistics could be read.
     */
    std::optional<PacketStats::Counters> get_packet_stats(Id reader_id, size_t stream_index);

    /**
     * @param reader_id The id of the reader to get the state for.
     * @param stream_index The index of the stream to get the state for.
     * @return The stream state for given reader.
     */
    [[nodiscard]] std::optional<StreamState> get_stream_state(Id reader_id, size_t stream_index) const;

    struct SocketWithContext {
        explicit SocketWithContext(boost::asio::io_context& io_context) : socket(io_context) {}

        AtomicRwLock rw_lock;
        udp_socket socket;
        uint16_t port {};
    };

    struct PacketBuffer {
        uint32_t timestamp;
        uint16_t seq;
        uint16_t data_len;
        std::array<uint8_t, aes67::constants::k_max_payload> payload;
    };

    struct StreamContext {
        Session session;
        Filter filter;
        uint16_t packet_time_frames {};
        ip_address_v4 interface;
        FifoBuffer<PacketBuffer, Fifo::Spsc> packets;
        FifoBuffer<uint16_t, Fifo::Spsc> packets_too_old;
        PacketStats packet_stats;
        boost::lockfree::spsc_value<PacketStats::Counters, boost::lockfree::allow_multiple_reads<true>>
            packet_stats_counters;
        SlidingStats packet_interval_stats {1000};  // For calculating jitter
        WrappingUint64 prev_packet_time_ns;
        std::atomic<StreamState> state {StreamState::inactive};
    };

    /**
     * Holds the structures to receive incoming data from redundant sources into a single buffer.
     */
    struct Reader {
        AtomicRwLock rw_lock;
        Id id;
        AudioFormat audio_format;
        std::array<StreamContext, k_max_num_redundant_sessions> streams;

        // Network thread
        std::optional<WrappingUint32> rtp_ts;
        WrappingUint16 seq;

        // Audio thread
        Ringbuffer receive_buffer;
        std::vector<uint8_t> read_audio_data_buffer;
        std::optional<WrappingUint32> most_recent_ts;  // ts of the latest received data
        WrappingUint32 next_ts_to_read;
    };

    /// Function for joining a multicast group. Can be overridden to alter behaviour. Used for unit testing.
    SafeFunction<bool(udp_socket&, ip_address_v4, ip_address_v4)> join_multicast_group;

    /// Function for leaving a multicast group. Can be overridden to alter behaviour. Used for unit testing.
    SafeFunction<bool(udp_socket&, ip_address_v4, ip_address_v4)> leave_multicast_group;

    static constexpr auto k_max_num_sessions = k_max_num_readers * k_max_num_redundant_sessions;

    boost::container::static_vector<SocketWithContext, k_max_num_sessions> sockets;
    boost::container::static_vector<Reader, k_max_num_readers> readers;

    uint64_t last_time_maintenance {};
};

/**
 * @return A string representation of ReceiveState.
 */
[[nodiscard]] const char* to_string(AudioReceiver::StreamState state);

}  // namespace rav::rtp
