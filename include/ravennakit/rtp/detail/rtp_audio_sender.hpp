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

#include "rtp_ringbuffer.hpp"
#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/core/audio/audio_buffer_view.hpp"
#include "ravennakit/core/audio/audio_format.hpp"
#include "ravennakit/core/containers/byte_buffer.hpp"
#include "ravennakit/core/net/asio/asio_helpers.hpp"
#include "ravennakit/core/sync/atomic_rw_lock.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/rtp/rtp_packet.hpp"

#include <boost/container/static_vector.hpp>

namespace rav::rtp {

struct AudioSender {
    /// The number of packet buffers available for sending. This value means that n packets worth of data can be queued
    /// for sending.
    static constexpr uint32_t k_buffer_num_packets = 100;

    /// The max number of frames to feed into the sender (using send_audio_data_realtime). This will usually correspond
    /// to an audio device buffer size.
    static constexpr uint32_t k_max_num_frames = 4096;

    /// List of supported audio encodings for the sender.
    static constexpr auto k_supported_encodings = {AudioEncoding::pcm_s16, AudioEncoding::pcm_s24};

    /// The maximum number of writers.
    static constexpr auto k_max_num_writers = 16;

    /// The maximum number of redundant sessions per stream.
    static constexpr auto k_max_num_redundant_sessions = 2;  // How many redundant paths

    using ArrayOfAddresses = std::array<ip_address_v4, k_max_num_redundant_sessions>;

    struct WriterParameters {
        AudioFormat audio_format;
        std::array<udp_endpoint, 2> destinations;
        uint32_t packet_time_frames {};
        uint8_t ttl{15};
        uint8_t payload_type {};
    };

    explicit AudioSender(boost::asio::io_context& io_context);
    ~AudioSender();

    /**
     * Adds a writer to the sender.
     * Thread safe: no.
     * @param id The id to use, must be unique.
     * @param parameters The parameters of the writer.
     * @param interfaces The interfaces for outbound.
     * @return true if a new writer was added, or false if not.
     */
    [[nodiscard]] bool add_writer(Id id, const WriterParameters& parameters, const ArrayOfAddresses& interfaces);

    /**
     * Removes the writer with given id, if it exists.
     * Thread safe: no.
     * @param id The id of the writer to remove.
     * @return true if a writer was removed, or false if not.
     */
    [[nodiscard]] bool remove_writer(Id id);

    /**
     * Sets the outbound interfaces on all sockets.
     * @param interfaces The new interfaces to use.
     */
    [[nodiscard]] bool set_interfaces(const ArrayOfAddresses& interfaces);

    /**
     * Sets the ttl of the sockets.
     * @param id The writer id.
     * @param ttl The new ttl.
     * @return True is writer was found and no error occurred, or false otherwise.
     */
    bool set_ttl(Id id, uint8_t ttl);

    /**
     * Call this to send outgoing packets onto the network. Should be called from a single high priority thread with
     * regular short intervals.
     */
    void send_outgoing_packets();

    /**
     * Schedules data for sending. A call to this function is realtime safe and thread safe as long as only one thread
     * makes the call.
     * @param id The id of the writer.
     * @param buffer The buffer to send.
     * @param timestamp The timestamp of the buffer.
     * @returns True if the buffer was sent, or false if something went wrong.
     */
    [[nodiscard]] bool send_data_realtime(Id id, BufferView<const uint8_t> buffer, uint32_t timestamp);

    /**
     * Schedules audio data for sending. A call to this function is realtime safe and thread safe as long as only one
     * thread makes the call.
     * @param id The id of the writer.
     * @param input_buffer The buffer to send.
     * @param timestamp The timestamp of the buffer.
     * @return True if the buffer was sent, or false if something went wrong.
     */
    [[nodiscard]] bool send_audio_data_realtime(Id id, const AudioBufferView<const float>& input_buffer, uint32_t timestamp);

    struct FifoPacket {
        uint32_t rtp_timestamp {};
        uint32_t payload_size_bytes {};
        std::array<uint8_t, aes67::constants::k_max_payload> payload {};
    };

    struct Writer {
        explicit Writer(std::array<udp_socket, k_max_num_redundant_sessions>&& s) : sockets(std::move(s)) {}

        AtomicRwLock rw_lock;
        Id id;
        std::array<udp_endpoint, k_max_num_redundant_sessions> destinations;
        std::array<udp_socket, k_max_num_redundant_sessions> sockets;
        std::atomic<size_t> num_packets_failed_to_schedule {0};  // TODO: Report somewhere
        std::atomic<size_t> num_packets_failed_to_send {0};      // TODO: Report somewhere

        // Audio thread:
        ByteBuffer rtp_packet_buffer;
        std::vector<uint8_t> intermediate_send_buffer;
        std::vector<uint8_t> intermediate_audio_buffer;
        uint32_t packet_time_frames {};
        Packet rtp_packet;
        AudioFormat audio_format;
        Ringbuffer rtp_buffer;

        // Audio thread writes and network thread reads:
        FifoBuffer<FifoPacket, Fifo::Spsc> outgoing_data;
    };

    struct SocketWithContext {
        explicit SocketWithContext(boost::asio::io_context& io_context) : socket(io_context) {}

        AtomicRwLock rw_lock;
        udp_socket socket;
    };

    boost::container::static_vector<Writer, k_max_num_writers> writers;
    boost::system::error_code last_error;  // Used to avoid log spamming
};

}  // namespace rav::rtp
