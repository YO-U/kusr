#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include <chrono>
#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) workers.emplace_back(fn);
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("tick-period,t", po::value<unsigned>(), "set tick period")
            ("config-file,c", po::value<std::string>(), "set config file path")
            ("www-root,w", po::value<std::string>(), "set static files root")
            ("randomize-spawn-points", "spawn dogs at random positions");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        if (!vm.count("config-file") || !vm.count("www-root")) {
            std::cerr << "Usage: game_server -c <config> -w <www-root> [-t <tick-period>] [--randomize-spawn-points]"sv << std::endl;
            return EXIT_FAILURE;
        }

        fs::path config_path(vm["config-file"].as<std::string>());
        fs::path static_root(vm["www-root"].as<std::string>());
        bool randomize_spawn = vm.count("randomize-spawn-points") > 0;

        model::Game game = json_loader::LoadGame(config_path);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int sn) {
            if (!ec) ioc.stop();
        });

        bool auto_tick = vm.count("tick-period") > 0;
        http_handler::RequestHandler handler{game, static_root, auto_tick};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "Server has started..."sv << std::endl;

        if (auto_tick) {
            auto tick_period = vm["tick-period"].as<unsigned>();
            auto ticker = std::make_shared<net::steady_timer>(ioc, std::chrono::milliseconds(tick_period));
            std::function<void()> tick_fn = [&game, ticker, tick_period, &tick_fn]() {
                for (auto& m : game.GetMaps()) {
                    auto* s = game.FindSession(m.GetId());
                    if (s) s->Tick(static_cast<int>(tick_period));
                }
                ticker->expires_after(std::chrono::milliseconds(tick_period));
                ticker->async_wait([&tick_fn](const sys::error_code& ec) {
                    if (!ec) tick_fn();
                });
            };
            ticker->async_wait([&tick_fn](const sys::error_code& ec) {
                if (!ec) tick_fn();
            });
        }

        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
