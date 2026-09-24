#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers; workers.reserve(n-1);
    while(--n) workers.emplace_back(fn);
    fn();
}

}

int main(int argc, const char* argv[]) {
    if(argc!=3){std::cerr<<"Usage: game_server <game-config-json> <static-root>"sv<<std::endl;return EXIT_FAILURE;}
    try{
        fs::path cp(argv[1]); fs::path sr(argv[2]);
        model::Game game=json_loader::LoadGame(cp);
        const unsigned nt=std::thread::hardware_concurrency();
        net::io_context ioc(nt);
        net::signal_set signals(ioc,SIGINT,SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec,[[maybe_unused]]int sn){if(!ec)ioc.stop();});
        http_handler::RequestHandler handler{game,sr};
        const auto addr=net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port=8080;
        http_server::ServeHttp(ioc,{addr,port},[&handler](auto&& req,auto&& send){handler(std::forward<decltype(req)>(req),std::forward<decltype(send)>(send));});
        std::cout<<"Server has started..."sv<<std::endl;
        RunWorkers(std::max(1u,nt),[&ioc]{ioc.run();});
    }catch(const std::exception& ex){std::cerr<<ex.what()<<std::endl;return EXIT_FAILURE;}
}
