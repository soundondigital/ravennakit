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

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/util/path_matcher.hpp"

#include <boost/beast.hpp>

#include <string_view>
#include <functional>
#include <map>
#include <utility>

namespace rav {

/**
 * A simple HTTP router that matches HTTP requests to handlers based on the method and path.
 * @tparam HandlerType The type of the handler function.
 */
template<typename HandlerType>
class HttpRouter {
  public:
    /**
     * Inserts a new route into the router. If a handler already exists for the given method and pattern, it will be
     * updated.
     * @param method The HTTP method to match (e.g., GET, POST).
     * @param pattern The path pattern to match.
     * @param handler The handler function to call when the route is matched.
     */
    void insert(const boost::beast::http::verb method, const std::string_view pattern, HandlerType handler) {
        RAV_ASSERT(!pattern.empty(), "Pattern cannot be empty");
        RAV_ASSERT(handler != nullptr, "Handler cannot be null");
        RAV_ASSERT(method != boost::beast::http::verb::unknown, "Method cannot be unknown");

        for (auto& route : routes_) {
            if (route.method == method && route.pattern == pattern) {
                route.handler = std::move(handler);
                return;  // Updated existing route
            }
        }

        // If no existing route was found, add a new one
        routes_.emplace_back(Route {.method = method, .pattern = std::string(pattern), .handler = std::move(handler)});
    }

    /**
     * Matches the given method and path to a handler. If a matching route is found, the handler is returned.
     * @param method The HTTP method to match (e.g., GET, POST).
     * @param path The path to match.
     * @return A pointer to the matching handler, or nullptr if no match is found.
     */
    HandlerType* match(const boost::beast::http::verb method, const std::string_view path) {
        for (auto& route : routes_) {
            if (route.method == method && PathMatcher::match(path, route.pattern)) {
                return &route.handler;
            }
        }
        return nullptr;  // No matching route found
    }

  private:
    struct Route {
        boost::beast::http::verb method {};
        std::string pattern;
        HandlerType handler;
    };

    std::vector<Route> routes_;
};

}  // namespace rav
