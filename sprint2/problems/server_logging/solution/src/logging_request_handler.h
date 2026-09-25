#pragma once

#include "request_handler.h"
#include "logging.h"

#include <chrono>
#include <string_view>

namespace http_handler {

namespace net = boost::asio;
using tcp = net::ip::tcp;

template <typename RequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(RequestHandler& decorated)
        : decorated_{decorated} {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(tcp::endpoint endpoint,
                    http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {
        auto start = std::chrono::steady_clock::now();
        LogRequest(endpoint, req);

        decorated_(std::move(req),
                   [endpoint, start, send = std::forward<Send>(send)](auto&& response) mutable {
                       auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
                       LogResponse(endpoint, response, ms);
                       send(std::forward<decltype(response)>(response));
                   });
    }

private:
    template <typename Request>
    static void LogRequest(const tcp::endpoint& endpoint, const Request& req) {
        using namespace std::literals;
        server_logging::json::object data;
        data["ip"] = endpoint.address().to_string();
        data["URI"] = std::string(req.target());
        data["method"] = std::string(req.method_string());
        server_logging::LogInfo(data, "request received"sv);
    }

    template <typename Response>
    static void LogResponse(const tcp::endpoint& endpoint, const Response& response, int64_t response_time_ms) {
        using namespace std::literals;
        server_logging::json::object data;
        data["ip"] = endpoint.address().to_string();
        data["response_time"] = response_time_ms;
        data["code"] = response.result_int();
        auto it = response.find(http::field::content_type);
        if (it != response.end()) {
            data["content_type"] = std::string(it->value());
        } else {
            data["content_type"] = nullptr;
        }
        server_logging::LogInfo(data, "response sent"sv);
    }

    RequestHandler& decorated_;
};

}  // namespace http_handler
