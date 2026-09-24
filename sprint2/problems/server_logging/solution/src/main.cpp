#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace logging = boost::log;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

namespace {

void InitLogging() {
    logging::add_common_attributes();
    auto sink = logging::add_console_log(std::cout);
    sink->locked_backend()->auto_flush(true);
    sink->set_formatter([](const logging::record_view& rec, logging::formatting_ostream& strm) {
        json::object obj;
        if (auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec)) {
            obj["timestamp"] = boost::posix_time::to_iso_extended_string(*ts);
        }
        if (auto data = logging::extract<json::value>("AdditionalData", rec)) {
            obj["data"] = *data;
        } else {
            obj["data"] = json::object{};
        }
        if (auto msg = logging::extract<std::string>("Message", rec)) {
            obj["message"] = *msg;
        }
        strm << json::serialize(obj);
    });
}

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
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-root>"sv << std::endl;
        return EXIT_FAILURE;
    }
    try {
        InitLogging();
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
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send, const std::string& ip) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), ip);
        });

        json::object start_data;
        start_data["port"] = port;
        start_data["address"] = address.to_string();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value(start_data)) << "server started"sv;

        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

        json::object exit_data;
        exit_data["code"] = 0;
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value(exit_data)) << "server exited"sv;
    } catch (const std::exception& ex) {
        json::object exit_data;
        exit_data["code"] = EXIT_FAILURE;
        exit_data["exception"] = ex.what();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value(exit_data)) << "server exited"sv;
        return EXIT_FAILURE;
    }
}
