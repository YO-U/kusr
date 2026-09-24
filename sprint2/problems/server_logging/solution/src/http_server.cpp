#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

namespace http_server {

namespace json = boost::json;
namespace logging = boost::log;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

void ReportError(beast::error_code ec, std::string_view what) {
    using namespace std::literals;
    if (what != "read"sv && what != "write"sv && what != "accept"sv) {
        return;
    }
    json::object data;
    data["code"] = ec.value();
    data["text"] = ec.message();
    data["where"] = std::string(what);
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value(data)) << "error"sv;
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
    using namespace std::literals;
    request_ = {};
    stream_.expires_after(30s);
    http::async_read(stream_, buffer_, request_,
                     beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read"sv);
    }
    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    using namespace std::literals;
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        ReportError(ec, "shutdown"sv);
    }
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) {
        return ReportError(ec, "write"sv);
    }
    if (close) {
        return Close();
    }
    Read();
}

}  // namespace http_server