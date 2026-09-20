#include "cafeteria.h"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>

namespace {

using namespace std::chrono_literals;

class HotDogOrder : public std::enable_shared_from_this<HotDogOrder> {
public:
    HotDogOrder(net::io_context& io, net::strand<net::io_context::executor_type>& strand, Store& store,
                std::shared_ptr<GasCooker> gas_cooker, int id, HotDogHandler handler)
        : io_{io}
        , strand_{strand}
        , store_{store}
        , gas_cooker_{std::move(gas_cooker)}
        , id_{id}
        , handler_{std::move(handler)}
        , sausage_timer_{io_}
        , bread_timer_{io_} {
    }

    void Start() {
        sausage_ = store_.GetSausage();
        bread_ = store_.GetBread();

        sausage_->StartFry(*gas_cooker_, [self = shared_from_this()] {
            self->OnSausageStarted();
        });
        bread_->StartBake(*gas_cooker_, [self = shared_from_this()] {
            self->OnBreadStarted();
        });
    }

private:
    void OnSausageStarted() {
        sausage_timer_.expires_after(1500ms);
        sausage_timer_.async_wait(
            net::bind_executor(strand_, [self = shared_from_this()](boost::system::error_code ec) {
                if (ec) {
                    return;
                }
                self->sausage_->StopFry();
                self->sausage_ready_ = true;
                self->TryDeliver();
            }));
    }

    void OnBreadStarted() {
        bread_timer_.expires_after(1000ms);
        bread_timer_.async_wait(
            net::bind_executor(strand_, [self = shared_from_this()](boost::system::error_code ec) {
                if (ec) {
                    return;
                }
                self->bread_->StopBaking();
                self->bread_ready_ = true;
                self->TryDeliver();
            }));
    }

    void TryDeliver() {
        if (delivered_ || !sausage_ready_ || !bread_ready_) {
            return;
        }
        delivered_ = true;
        try {
            handler_(HotDog{id_, sausage_, bread_});
        } catch (...) {
            handler_(Result<HotDog>::FromCurrentException());
        }
    }

    net::io_context& io_;
    net::strand<net::io_context::executor_type>& strand_;
    Store& store_;
    std::shared_ptr<GasCooker> gas_cooker_;
    int id_;
    HotDogHandler handler_;
    net::steady_timer sausage_timer_;
    net::steady_timer bread_timer_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;
    bool sausage_ready_ = false;
    bool bread_ready_ = false;
    bool delivered_ = false;
};

}  // namespace

void Cafeteria::OrderHotDog(HotDogHandler handler) {
    net::post(strand_, [this, handler = std::move(handler)]() mutable {
        const int id = ++next_hotdog_id_;
        std::make_shared<HotDogOrder>(io_, strand_, store_, gas_cooker_, id, std::move(handler))->Start();
    });
}
