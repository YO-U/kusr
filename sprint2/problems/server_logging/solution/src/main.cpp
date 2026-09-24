#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes.hpp>
#include <boost/json.hpp>
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
namespace logging = boost::log;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp_attr, "TimeStamp", boost::posix_time::ptime)

namespace {

void InitLogging() {
    logging::core::get()->remove_all_sinks();
    auto sink = logging::add_console_log(std::cout);
    sink->set_formatter([](logging::record_view const& rec, logging::formatting_ostream& strm) {
        json::object obj;
        if (auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec)) {
            obj["timestamp"] = boost::posix_time::to_iso_extended_string(*ts) + "Z";
        }
        if (auto msg = logging::extract<std::string>("Message", rec)) {
            obj["message"] = *msg;
        }
        if (auto data = logging::extract<json::value>("AdditionalData", rec)) {
            obj["data"] = *data;
        }
        strm << json::serialize(obj) << std::endl;
    });
    logging::add_common_attributes();
}

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers; workers.reserve(n-1);
    while(--n) workers.emplace_back(fn);
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    InitLogging();
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
        if(vm.count("help")){std::cout<<desc<<std::endl;return 0;}
        if(!vm.count("config-file")||!vm.count("www-root")){
            std::cerr<<"Usage: game_server -c <config> -w <www-root> [-t <tick-period>]"sv<<std::endl;
            return EXIT_FAILURE;
        }

        fs::path cp(vm["config-file"].as<std::string>());
        fs::path sr(vm["www-root"].as<std::string>());
        model::Game game = json_loader::LoadGame(cp);

        const unsigned nt = std::thread::hardware_concurrency();
        net::io_context ioc(nt);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int sn) { if(!ec) ioc.stop(); });

        bool auto_tick = vm.count("tick-period") > 0;
        http_handler::RequestHandler handler{game, sr, auto_tick};

        const auto addr = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {addr, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        json::value start_data{{"port", 8080}, {"address", "0.0.0.0"}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, start_data) << "server started"sv;

        if(auto_tick) {
            auto tp = vm["tick-period"].as<unsigned>();
            auto ticker = std::make_shared<net::steady_timer>(ioc, std::chrono::milliseconds(tp));
            std::function<void()> tf = [&game, ticker, tp, &tf]() {
                for(auto& m : game.GetMaps()) { auto* s = game.FindSession(m.GetId()); if(s) s->Tick(static_cast<int>(tp)); }
                ticker->expires_after(std::chrono::milliseconds(tp));
                ticker->async_wait([&tf](const sys::error_code& ec) { if(!ec) tf(); });
            };
            ticker->async_wait([&tf](const sys::error_code& ec) { if(!ec) tf(); });
        }

        RunWorkers(std::max(1u, nt), [&ioc] { ioc.run(); });

        json::value exit_data{{"code", 0}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, exit_data) << "server exited"sv;
    } catch(const std::exception& ex) {
        json::value err_data{{"code", 1}, {"text", ex.what()}, {"where", "main"}};
        BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, err_data) << "error"sv;
        return EXIT_FAILURE;
    }
}
