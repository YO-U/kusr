#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io}
        , strand_{net::make_strand(io_)} {
    }

    void OrderHotDog(HotDogHandler handler);

private:
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    int next_hotdog_id_ = 0;
};
