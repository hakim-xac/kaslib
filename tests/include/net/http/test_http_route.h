/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_HTTP_ROUTE_TESTS_H
#define KASLIBCXX_NET_HTTP_HTTP_ROUTE_TESTS_H

#include "KAS/expected/expected.h"
#include "KAS/net/http/HttpRoute.h"
#include "KAS/net/http/Methods.h"
#include "KAS/net/http/Request.h"
#include "KAS/net/http/Response.h"
#include "tests/tests.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kas::net::http::http_route::tests {

namespace {
    using namespace kas::tests;
    using kas::unexpected_string;
    using kas::net::http::HttpRoute;
    using kas::net::http::Methods;
    using kas::net::http::Request;
    using kas::net::http::Response;

    // ------------------------------------------------------------
    // Helper factories
    // ------------------------------------------------------------

    /// Builds a Request with the given method.
    Request makeRequest(std::string method)
    {
        Request req;
        req.setMethod(std::move(method));
        return req;
    }

    // Result checks for call(): success or a specific error.

    /// Checks that the result is success.
    bool isOk(const expected_string<void>& r)
    {
        return r.has_value();
    }

    /// Checks that the result is the "no handler" error.
    bool isNoHandler(const expected_string<void>& r)
    {
        return r.has_value()
            ? false
            : r.error() == HttpRoute::ERROR_NO_HANDLER;
    }

    /// Checks that the result is the "method not allowed" error.
    bool isMethodNotAllowed(const expected_string<void>& r)
    {
        return r.has_value()
            ? false
            : r.error() == HttpRoute::ERROR_METHOD_NOT_ALLOWED;
    }

    /// Marker handler that records that it was invoked.
    /// NOTE: now returns expected_string<void> - success is {}.
    struct HandlerProbe {
        bool called = false;
        std::string last_method;

        /// Creates a handler that records the call and the request method.
        HttpRoute::end_point_handler_t make()
        {
            return [this](const Request& req, Response&) -> expected_string<void> {
                called = true;
                last_method = std::string(req.getMethod());
                return { };
            };
        }
    };

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all HttpRoute tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::http::http_route::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    // --------------------------------------------------------
    // 1. Default constructor: no handler,
    //    call() returns the "no handler" error.
    // --------------------------------------------------------
    {
        HttpRoute route;
        Request req = makeRequest("GET");
        Response resp;
        auto r = route.call(req, resp);
        KAS_TEST_CHECK_IMPL(ctx, !r.has_value());
        KAS_TEST_CHECK_IMPL(ctx, isNoHandler(r));
    }

    // --------------------------------------------------------
    // 2. Constructor with handler: the handler is invoked;
    //    all methods are allowed by default (m_allowed_methods is empty).
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route { probe.make() };

        Response resp;
        auto r1 = route.call(makeRequest("GET"), resp);
        KAS_TEST_CHECK_IMPL(ctx, isOk(r1));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);
        KAS_TEST_CHECK_IMPL(ctx, probe.last_method == "GET");

        probe.called = false;
        auto r2 = route.call(makeRequest("POST"), resp);
        KAS_TEST_CHECK_IMPL(ctx, isOk(r2));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        auto r3 = route.call(makeRequest("TRACE"), resp);
        KAS_TEST_CHECK_IMPL(ctx, isOk(r3));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        auto r4 = route.call(makeRequest("CONNECT"), resp);
        KAS_TEST_CHECK_IMPL(ctx, isOk(r4));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);
    }

    // --------------------------------------------------------
    // 3. setMethod<GET>: GET passes, POST returns 405.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setMethod<Methods::GET>().setHandler(probe.make());

        Response resp;

        probe.called = false;
        auto r1 = route.call(makeRequest("GET"), resp);
        KAS_TEST_CHECK_IMPL(ctx, isOk(r1));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        auto r2 = route.call(makeRequest("POST"), resp);
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(r2));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);

        probe.called = false;
        auto r3 = route.call(makeRequest("HEAD"), resp);
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(r3));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);
    }

    // --------------------------------------------------------
    // 4. setMethod<ALL>: all standard methods pass.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setMethod<Methods::ALL>().setHandler(probe.make());

        const std::vector<std::string_view> all {
            "GET", "HEAD", "POST", "PUT", "PATCH",
            "DELETE", "OPTIONS", "TRACE", "CONNECT"
        };

        Response resp;
        for (auto m : all) {
            probe.called = false;
            auto r = route.call(makeRequest(std::string { m }), resp);
            KAS_TEST_CHECK_IMPL(ctx, isOk(r));
            KAS_TEST_CHECK_IMPL(ctx, probe.called);
        }

        // Non-standard method -> 405.
        probe.called = false;
        auto r = route.call(makeRequest("PURGE"), resp);
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(r));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);
    }

    // --------------------------------------------------------
    // 5. setAllowedMethods<GET, POST>: only GET and POST.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setAllowedMethods<Methods::GET, Methods::POST>()
            .setHandler(probe.make());

        Response resp;

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("POST"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("PUT"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("DELETE"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);
    }

    // --------------------------------------------------------
    // 6. setAllowedMethods<> (empty pack): m_allowed_methods is empty,
    //    which by contract means "everything is allowed".
    //    NOTE: this is the semantics of the current call() implementation:
    //    if (!m_allowed_methods.empty() && !allows(...)) -> 405.
    //    An empty list skips the check, so the handler is invoked.
    //    The test pins this contract.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setAllowedMethods<>().setHandler(probe.make());

        Response resp;
        for (std::string_view m : { "GET", "POST", "HEAD", "OPTIONS" }) {
            probe.called = false;
            auto r = route.call(makeRequest(std::string { m }), resp);
            KAS_TEST_CHECK_IMPL(ctx, isOk(r));
            KAS_TEST_CHECK_IMPL(ctx, probe.called);
        }

        KAS_TEST_CHECK_IMPL(ctx, route.getAllowedMethods().empty());
    }

    // --------------------------------------------------------
    // 7. setAllowedMethods overrides a previous setMethod.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setMethod<Methods::GET>()
            .setAllowedMethods<Methods::POST>()
            .setHandler(probe.make());

        Response resp;

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("POST"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);
    }

    // --------------------------------------------------------
    // 8. setMethod overrides a previous setAllowedMethods.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setAllowedMethods<Methods::GET, Methods::POST>()
            .setMethod<Methods::DELETE>()
            .setHandler(probe.make());

        Response resp;

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("DELETE"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);
    }

    // --------------------------------------------------------
    // 9. setHandler after setMethod (lvalue chain).
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setMethod<Methods::PUT>();
        route.setHandler(probe.make());

        Response resp;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("PUT"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);
    }

    // --------------------------------------------------------
    // 10. Handler replacement: the last installed handler is used.
    // --------------------------------------------------------
    {
        HandlerProbe first;
        HandlerProbe second;
        HttpRoute route;
        route.setMethod<Methods::GET>()
            .setHandler(first.make())
            .setHandler(second.make());

        Response resp;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !first.called);
        KAS_TEST_CHECK_IMPL(ctx, second.called);
    }

    // --------------------------------------------------------
    // 11. Rvalue chain: setMethod() && + setHandler() &&.
    // --------------------------------------------------------
    {
        HandlerProbe probe;

        HttpRoute route = HttpRoute { }
                              .setMethod<Methods::PATCH>()
                              .setHandler(probe.make());

        Response resp;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("PATCH"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);
    }

    // --------------------------------------------------------
    // 12. Rvalue chain with setMethod<ALL>() &&.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route = HttpRoute { }
                              .setMethod<Methods::ALL>()
                              .setHandler(probe.make());

        Response resp;
        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("OPTIONS"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("UNKNOWN"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);
    }

    // --------------------------------------------------------
    // 13. Method casing is significant: "get" != "GET".
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setMethod<Methods::GET>().setHandler(probe.make());

        Response resp;

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("get"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);
    }

    // --------------------------------------------------------
    // 14. call() on a const object (const-correctness).
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        const HttpRoute route = HttpRoute { }
                                    .setMethod<Methods::GET>()
                                    .setHandler(probe.make());

        Response resp;
        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);
    }

    // --------------------------------------------------------
    // 15. setMethod<ALL>() followed by setMethod<GET>() keeps only GET.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute route;
        route.setMethod<Methods::ALL>()
            .setMethod<Methods::GET>()
            .setHandler(probe.make());

        Response resp;

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("POST"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, !probe.called);
    }

    // --------------------------------------------------------
    // 16. The handler is invoked exactly once per call().
    // --------------------------------------------------------
    {
        int call_count = 0;
        HttpRoute route;
        route.setMethod<Methods::GET>().setHandler(
            [&call_count](const Request&, Response&) -> expected_string<void> {
                ++call_count;
                return { };
            });

        Response resp;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, call_count == 1);

        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, call_count == 2);

        KAS_TEST_CHECK_IMPL(ctx, isMethodNotAllowed(route.call(makeRequest("POST"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, call_count == 2);
    }

    // --------------------------------------------------------
    // 17. MoveOnlyFunction: move-only capture (unique_ptr).
    // --------------------------------------------------------
    {
        auto marker = std::make_unique<int>(42);
        HttpRoute route;
        route.setMethod<Methods::GET>().setHandler(
            [m = std::move(marker)](const Request&, Response&) -> expected_string<void> {
                (void)*m;
                return { };
            });

        Response resp;
        KAS_TEST_CHECK_IMPL(ctx, isOk(route.call(makeRequest("GET"), resp)));
    }

    // --------------------------------------------------------
    // 18. Move HttpRoute: the destination works, the source is left in a
    //     moved-from state (calling it is not UB, but returns "no handler",
    //     since the optional<MoveOnlyFunction> has been moved away).
    //     The moved-from contract does not guarantee specific behavior,
    //     so we only check that dst works.
    // --------------------------------------------------------
    {
        HandlerProbe probe;
        HttpRoute src;
        src.setMethod<Methods::GET>().setHandler(probe.make());

        HttpRoute dst = std::move(src);

        Response resp;
        probe.called = false;
        KAS_TEST_CHECK_IMPL(ctx, isOk(dst.call(makeRequest("GET"), resp)));
        KAS_TEST_CHECK_IMPL(ctx, probe.called);

        // Moved-from src: must not be UB. We only check that the call does
        // not crash (the result is unspecified and not checked).
        (void)src.call(makeRequest("GET"), resp);
    }

    // --------------------------------------------------------
    // 19. An error returned from the handler is propagated as is.
    // --------------------------------------------------------
    {
        HttpRoute route;
        route.setMethod<Methods::POST>().setHandler(
            [](const Request&, Response&) -> expected_string<void> {
                return unexpected_string { "empty body" };
            });

        Response resp;
        auto r = route.call(makeRequest("POST"), resp);
        KAS_TEST_CHECK_IMPL(ctx, !r.has_value());
        KAS_TEST_CHECK_IMPL(ctx, r.error() == "empty body");
    }

    // --------------------------------------------------------
    // 20. getAllowedMethods() reflects the configured set.
    // --------------------------------------------------------
    {
        HttpRoute route;
        route.setAllowedMethods<Methods::GET, Methods::POST>();

        const auto& methods = route.getAllowedMethods();
        KAS_TEST_CHECK_IMPL(ctx, methods.size() == 2);
        KAS_TEST_CHECK_IMPL(ctx,
            std::find(methods.begin(), methods.end(), "GET") != methods.end());
        KAS_TEST_CHECK_IMPL(ctx,
            std::find(methods.begin(), methods.end(), "POST") != methods.end());

        // setMethod<> replaces the list.
        route.setMethod<Methods::DELETE>();
        KAS_TEST_CHECK_IMPL(ctx, route.getAllowedMethods().size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, route.getAllowedMethods().front() == "DELETE");

        // setMethod<ALL> expands to 9 methods.
        route.setMethod<Methods::ALL>();
        KAS_TEST_CHECK_IMPL(ctx, route.getAllowedMethods().size() == 9);
    }

    // --------------------------------------------------------
    // 21. hasHandler() / allows() - public getters.
    // --------------------------------------------------------
    {
        HttpRoute route;
        KAS_TEST_CHECK_IMPL(ctx, !route.hasHandler());
        KAS_TEST_CHECK_IMPL(ctx, route.getAllowedMethods().empty());

        route.setMethod<Methods::GET>();
        KAS_TEST_CHECK_IMPL(ctx, route.allows("GET"));
        KAS_TEST_CHECK_IMPL(ctx, !route.allows("POST"));

        HandlerProbe probe;
        route.setHandler(probe.make());
        KAS_TEST_CHECK_IMPL(ctx, route.hasHandler());
    }

    return printSummary("kas::net::http::http_route::tests", ctx);
}

} // namespace kas::net::http::http_route::tests

#endif // KASLIBCXX_NET_HTTP_HTTP_ROUTE_TESTS_H