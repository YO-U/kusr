#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp>

#include <iostream>
#include <thread>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"
#include "logging_request_handler.h"
#include "logging.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace json = boost::json;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    server_logging::InitFormatter();
    try {
        if (argc != 3) {
            std::cerr << "Usage: game_server <game-config-json> <static-root>"sv << std::endl;
            return EXIT_FAILURE;
        }

        fs::path config_path(argv[1]);
        fs::path static_root(argv[2]);
        model::Game game = json_loader::LoadGame(config_path);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        http_handler::RequestHandler handler{game, static_root};
        http_handler::LoggingRequestHandler logging_handler{handler};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port},
                               [&logging_handler](auto&& endpoint, auto&& req, auto&& send) {
                                   logging_handler(std::forward<decltype(endpoint)>(endpoint),
                                                   std::forward<decltype(req)>(req),
                                                   std::forward<decltype(send)>(send));
                               });

        json::value start_data{{"port", port}, {"address", address.to_string()}};
        server_logging::LogInfo(start_data, "server started"sv);

        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

        json::value exit_data{{"code", 0}};
        server_logging::LogInfo(exit_data, "server exited"sv);
    } catch (const std::exception& ex) {
        json::value err{{"code", EXIT_FAILURE}, {"exception", ex.what()}};
        server_logging::LogInfo(err, "server exited"sv);
        return EXIT_FAILURE;
    }
}
