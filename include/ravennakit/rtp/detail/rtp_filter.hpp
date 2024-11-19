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

#include "ravennakit/sdp/constants.hpp"
#include "ravennakit/sdp/session_description.hpp"

#include <asio.hpp>
#include <tl/expected.hpp>

namespace rav {

/**
 * Implements logic for filtering RTP packets.
 */
class rtp_filter {
  public:
    rtp_filter() = default;

    /**
     * Creates a new RTP filter for the given connection address.
     * @param connection_address The connection address.
     */
    explicit rtp_filter(asio::ip::address connection_address) : connection_address_(std::move(connection_address)) {}

    rtp_filter(const rtp_filter&) = default;
    rtp_filter& operator=(const rtp_filter&) = default;

    rtp_filter(rtp_filter&&) noexcept = default;
    rtp_filter& operator=(rtp_filter&&) noexcept = default;

    /**
     * Adds a filter for the given source address.
     * @param src_address The source address.
     * @param mode The filter mode.
     */
    void add_filter(asio::ip::address src_address, const sdp::filter_mode mode) {
        RAV_TRACE(
            "Added source filter: {} {} {}", rav::sdp::to_string(mode), connection_address_.to_string(),
            src_address.to_string()
        );
        filters_.push_back({mode, std::move(src_address)});
    }

    /**
     * Adds a filter from the given source filter.
     * @param source_filter The source filter.
     * @return The number of source filters added.
     */
    size_t add_filter(const sdp::source_filter& source_filter) {
        size_t total = 0;
        const auto dest_address = asio::ip::make_address(source_filter.dest_address());
        if (dest_address != connection_address_) {
            return 0;
        }
        for (auto& src : source_filter.src_list()) {
            add_filter(asio::ip::make_address(src), source_filter.mode());
            total++;
        }
        return total;
    }

    /**
     * Adds filters from given vector of source filters.
     * @param filters The source filters.
     * @return The number of source filters added.
     */
    size_t add_filters(const std::vector<sdp::source_filter>& filters) {
        size_t total = 0;
        for (auto& f : filters) {
            total += add_filter(f);
        }
        return total;
    }

    /**
     * Checks if the given connection address matches the filter.
     * @param connection_address The connection address.
     * @return True if the connection address matches the filter, or false if not.
     */
    [[nodiscard]] bool matches(const asio::ip::address& connection_address) const {
        return connection_address_ == connection_address;
    }

    /**
     * Checks if the given connection address matches and if source address is a valid source address.
     * If the source address is not a valid source address, it will return false.
     * In case there are no filters, it will return true.
     * @param connection_address The connection address.
     * @param src_address The source address.
     * @return True if the connection address and source address matches the filter, or false if not.
     */
    [[nodiscard]] bool
    is_valid_source(const asio::ip::address& connection_address, const asio::ip::address& src_address) const {
        if (connection_address_ != connection_address) {
            return false;
        }

        if (filters_.empty()) {
            return true;
        }

        bool is_address_included = false;
        bool has_include_filters = false;

        for (auto& f : filters_) {
            if (f.mode == sdp::filter_mode::exclude && f.address == src_address) {
                return false;  // This prioritizes exclude filters over include filters
            }
            if (f.mode == sdp::filter_mode::include) {
                has_include_filters = true;
                if (f.address == src_address) {
                    is_address_included = true;
                }
            }
        }

        return has_include_filters ? is_address_included : true;
    }

    /**
     * @return True if the filter is empty, or false if not.
     */
    [[nodiscard]] bool empty() const {
        return filters_.empty();
    }

  private:
    struct filter {
        sdp::filter_mode mode {sdp::filter_mode::undefined};
        asio::ip::address address;
    };

    asio::ip::address connection_address_;
    std::vector<filter> filters_;
};

}  // namespace rav
