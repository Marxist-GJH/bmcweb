// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#pragma once

#include "bmcweb_config.h"

#include "async_resp.hpp"
#include "http_request.hpp"
#include "http_server.hpp"
#include "io_context_singleton.hpp"
#include "logging.hpp"
#include "routing.hpp"
#include "routing/dynamicrule.hpp"

#include <sys/socket.h>
#include <systemd/sd-daemon.h>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage, clang-diagnostic-unused-macros)
#define BMCWEB_ROUTE(app, url)                                                 \
    app.template route<crow::utility::getParameterTag(url)>(url)

namespace crow
{
class App
{
  public:
    using ssl_socket_t = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
    using raw_socket_t = boost::asio::ip::tcp::socket;

    using socket_type = std::conditional_t<BMCWEB_INSECURE_DISABLE_SSL,
                                           raw_socket_t, ssl_socket_t>;
    using server_type = Server<App, socket_type>;

    template <typename Adaptor>
    void handleUpgrade(const std::shared_ptr<Request>& req,
                       const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       Adaptor&& adaptor)
    {
        router.handleUpgrade(req, asyncResp, std::forward<Adaptor>(adaptor));
    }

    void handle(const std::shared_ptr<Request>& req,
                const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
    {
        router.handle(req, asyncResp);
    }

    DynamicRule& routeDynamic(const std::string& rule)
    {
        return router.newRuleDynamic(rule);
    }

    template <uint64_t Tag>
    auto& route(std::string&& rule)
    {
        return router.newRuleTagged<Tag>(std::move(rule));
    }

    void validate()
    {
        router.validate();
    }

    void loadCertificate()
    {
        BMCWEB_LOG_DEBUG("Loading certificate");
        if (!server)
        {
            return;
        }
        server->loadCertificate();
    }

    static std::optional<boost::asio::ip::tcp::acceptor> setupSocket()
    {
        constexpr int defaultPort = 18080;
        if (sd_listen_fds(0) == 1)
        {
            BMCWEB_LOG_INFO("attempting systemd socket activation");
            if (sd_is_socket_inet(SD_LISTEN_FDS_START, AF_UNSPEC, SOCK_STREAM,
                                  1, 0) != 0)
            {
                BMCWEB_LOG_INFO("Starting webserver on socket handle {}",
                                SD_LISTEN_FDS_START);
                return boost::asio::ip::tcp::acceptor(
                    getIoContext(), boost::asio::ip::tcp::v6(),
                    SD_LISTEN_FDS_START);
            }
            BMCWEB_LOG_ERROR(
                "bad incoming socket, starting webserver on port {}",
                defaultPort);
        }
        BMCWEB_LOG_INFO("Starting webserver on port {}", defaultPort);
        return boost::asio::ip::tcp::acceptor(
            getIoContext(),
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address("0.0.0.0"), defaultPort));
    }

    void run()
    {
        validate();

        std::optional<boost::asio::ip::tcp::acceptor> acceptor = setupSocket();
        if (!acceptor)
        {
            BMCWEB_LOG_CRITICAL("Couldn't start server");
            return;
        }
        server.emplace(this, std::move(*acceptor), sslContext, getIoContext());
        server->run();
    }

    void debugPrint()
    {
        BMCWEB_LOG_DEBUG("Routing:");
        router.debugPrint();
    }

    std::vector<const std::string*> getRoutes()
    {
        const std::string root;
        return router.getRoutes(root);
    }
    std::vector<const std::string*> getRoutes(const std::string& parent)
    {
        return router.getRoutes(parent);
    }

    App& ssl(std::shared_ptr<boost::asio::ssl::context>&& ctx)
    {
        sslContext = std::move(ctx);
        BMCWEB_LOG_INFO("app::ssl context use_count={}",
                        sslContext.use_count());
        return *this;
    }

    std::shared_ptr<boost::asio::ssl::context> sslContext = nullptr;

    std::optional<server_type> server;

    Router router;
};
} // namespace crow
using App = crow::App;
