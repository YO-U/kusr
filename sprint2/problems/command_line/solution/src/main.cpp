#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/program_options.hpp>
#include <boost/json.hpp>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <filesystem>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include "json_loader.h"
#include "request_handler.h"
#include "logging_request_handler.h"
#include "logging.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace po = boost::program_options;
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

struct ServerArgs {
    fs::path config_path;
    fs::path static_root;
    bool randomize_spawn = false;
    bool auto_tick = false;
    unsigned tick_period = 0;
};

std::optional<ServerArgs> ParseArgs(int argc, const char* argv[]) {
    // Positional form used by earlier sprint2 tasks / Dockerfiles:
    //   game_server <config.json> <static-root>
    if (argc == 3 && argv[1][0] != '-') {
        return ServerArgs{fs::path(argv[1]), fs::path(argv[2]), false, false, 0};
    }

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<unsigned>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value<std::string>()->value_name("file"), "set config file path")
        ("www-root,w", po::value<std::string>()->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    if (!vm.count("config-file") || !vm.count("www-root")) {
        std::cerr << "Usage: game_server --config-file <path> --www-root <path> [--tick-period <ms>] [--randomize-spawn-points]"sv
                  << std::endl;
        std::exit(EXIT_FAILURE);
    }

    ServerArgs args;
    args.config_path = fs::path(vm["config-file"].as<std::string>());
    args.static_root = fs::path(vm["www-root"].as<std::string>());
    args.randomize_spawn = vm.count("randomize-spawn-points") > 0;
    args.auto_tick = vm.count("tick-period") > 0;
    if (args.auto_tick) {
        args.tick_period = vm["tick-period"].as<unsigned>();
    }
    return args;
}

}  // namespace

int main(int argc, const char* argv[]) {
    server_logging::InitFormatter();
    try {
        auto parsed = ParseArgs(argc, argv);
        if (!parsed) {
            return EXIT_SUCCESS;
        }
        ServerArgs args = *parsed;

        model::Game game = json_loader::LoadGame(args.config_path);
        game.SetRandomizeSpawnPoints(args.randomize_spawn);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        http_handler::RequestHandler handler{game, args.static_root, args.auto_tick};
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

        if (args.auto_tick) {
            auto tick_period = args.tick_period;
            auto ticker = std::make_shared<net::steady_timer>(ioc);
            std::shared_ptr<std::function<void()>> tick_fn = std::make_shared<std::function<void()>>();
            *tick_fn = [&game, ticker, tick_period, tick_fn]() {
                for (auto& m : game.GetMaps()) {
                    if (auto* s = game.FindSession(m.GetId())) {
                        s->Tick(static_cast<int>(tick_period));
                    }
                }
                ticker->expires_after(std::chrono::milliseconds(tick_period));
                ticker->async_wait([tick_fn](const sys::error_code& ec) {
                    if (!ec) {
                        (*tick_fn)();
                    }
                });
            };
            ticker->expires_after(std::chrono::milliseconds(tick_period));
            ticker->async_wait([tick_fn](const sys::error_code& ec) {
                if (!ec) {
                    (*tick_fn)();
                }
            });
        }

        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

        json::value exit_data{{"code", 0}};
        server_logging::LogInfo(exit_data, "server exited"sv);
    } catch (const std::exception& ex) {
        json::value err{{"code", EXIT_FAILURE}, {"exception", ex.what()}};
        server_logging::LogInfo(err, "server exited"sv);
        return EXIT_FAILURE;
    }
}
