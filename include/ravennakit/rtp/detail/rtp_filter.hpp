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

#include "ravennakit/sdp/detail/sdp_constants.hpp"
#include "ravennakit/sdp/sdp_session_description.hpp"

#include <boost/asio.hpp>
#include "ravennakit/core/expected.hpp"

namespace rav::rtp {

/**
 * Implements logic for filtering RTP packets.
 */
class Filter {
  public:
    Filter() = default;

    /**
     * Creates a new RTP filter for the given connection address.
     * @param connection_address The connection address.
     */
    explicit Filter(boost::asio::ip::address connection_address) : connection_address_(std::move(connection_address)) {}

    /**
     * Convenience constructor to create a filter with a source address already added to it.
     * @param connection_address The connectin address.
     * @param src_address The source address.
     * @param mode Whether to include or exclude the source.
     */
    explicit Filter(
        boost::asio::ip::address connection_address, const boost::asio::ip::address& src_address,
        const sdp::FilterMode mode
    ) :
        connection_address_(std::move(connection_address)) {
        add_filter(src_address, mode);
    }

    Filter(const Filter&) = default;
    Filter& operator=(const Filter&) = default;

    Filter(Filter&&) noexcept = default;
    Filter& operator=(Filter&&) noexcept = default;

    /**
     * Adds a filter for the given source address.
     * @param src_address The source address.
     * @param mode The filter mode.
     */
    void add_filter(boost::asio::ip::address src_address, const sdp::FilterMode mode) {
        filters_.push_back({mode, std::move(src_address)});
    }

    /**
     * Adds a filter from the given source filter.
     * @param source_filter The source filter.
     * @return The number of source filters added.
     */
    size_t add_filter(const sdp::SourceFilter& source_filter) {
        size_t total = 0;
        const auto dest_address = boost::asio::ip::make_address(source_filter.dest_address);
        if (dest_address != connection_address_) {
            return 0;
        }
        for (auto& src : source_filter.src_list) {
            add_filter(boost::asio::ip::make_address(src), source_filter.mode);
            total++;
        }
        return total;
    }

    /**
     * Adds filters from given vector of source filters.
     * @param filters The source filters.
     * @return The number of source filters added.
     */
    size_t add_filters(const std::vector<sdp::SourceFilter>& filters) {
        size_t total = 0;
        for (auto& f : filters) {
            total += add_filter(f);
        }
        return total;
    }

    /**
     * @return The connection address.
     */
    [[nodiscard]] boost::asio::ip::address connection_address() const {
        return connection_address_;
    }

    /**
     * Checks if the given connection address matches and if source address is a valid source address.
     * If the source address is not a valid source address, it will return false.
     * In case there are no filters, it will return true.
     * @param connection_address The connection address.
     * @param src_address The source address.
     * @return True if the connection address and source address matches the filter, or false if not.
     */
    [[nodiscard]] bool is_valid_source(
        const boost::asio::ip::address& connection_address, const boost::asio::ip::address& src_address
    ) const {
        if (connection_address_ != connection_address) {
            return false;
        }

        if (filters_.empty()) {
            return true;
        }

        bool is_address_included = false;
        bool has_include_filters = false;

        for (auto& f : filters_) {
            if (f.mode == sdp::FilterMode::exclude && f.address == src_address) {
                return false;  // This prioritizes exclude filters over include filters
            }
            if (f.mode == sdp::FilterMode::include) {
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

    friend bool operator==(const Filter& lhs, const Filter& rhs) {
        return std::tie(lhs.connection_address_, lhs.filters_) == std::tie(rhs.connection_address_, rhs.filters_);
    }

    friend bool operator!=(const Filter& lhs, const Filter& rhs) {
        return !(lhs == rhs);
    }

  private:
    struct filter {
        sdp::FilterMode mode {sdp::FilterMode::undefined};
        boost::asio::ip::address address;

        friend bool operator==(const filter& lhs, const filter& rhs) {
            return std::tie(lhs.mode, lhs.address) == std::tie(rhs.mode, rhs.address);
        }

        friend bool operator!=(const filter& lhs, const filter& rhs) {
            return !(lhs == rhs);
        }
    };

    boost::asio::ip::address connection_address_;
    std::vector<filter> filters_; // TODO: Avoid allocations here. Store filters in-class.
};

}  // namespace rav::rtp
