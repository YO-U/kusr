#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <string_view>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::literals;

        std::string_view target = req.target();
        if (auto pos = target.find('?'); pos != std::string_view::npos) {
            target = target.substr(0, pos);
        }

        const auto json_response = [&req](http::status status, std::string body) {
            http::response<http::string_body> response(status, req.version());
            response.set(http::field::content_type, "application/json");
            response.keep_alive(req.keep_alive());
            response.body() = std::move(body);
            response.content_length(response.body().size());
            return response;
        };

        if (req.method() != http::verb::get) {
            send(json_response(http::status::bad_request,
                               R"({"code":"badRequest","message":"Bad request"})"));
            return;
        }

        if (target == "/api/v1/maps") {
            json::array maps;
            for (const auto& map : game_.GetMaps()) {
                maps.emplace_back(json::object{
                    {"id", *map.GetId()},
                    {"name", map.GetName()},
                });
            }
            send(json_response(http::status::ok, json::serialize(maps)));
            return;
        }

        constexpr std::string_view maps_prefix = "/api/v1/maps/";
        if (target.starts_with(maps_prefix)) {
            const std::string map_id{target.substr(maps_prefix.size())};
            if (map_id.empty() || map_id.find('/') != std::string::npos) {
                send(json_response(http::status::bad_request,
                                   R"({"code":"badRequest","message":"Bad request"})"));
                return;
            }

            const model::Map* map = game_.FindMap(model::Map::Id(map_id));
            if (!map) {
                send(json_response(http::status::not_found,
                                   R"({"code":"mapNotFound","message":"Map not found"})"));
                return;
            }

            send(json_response(http::status::ok, json::serialize(SerializeMap(*map))));
            return;
        }

        if (target.starts_with("/api/")) {
            send(json_response(http::status::bad_request,
                               R"({"code":"badRequest","message":"Bad request"})"));
            return;
        }

        send(json_response(http::status::bad_request,
                           R"({"code":"badRequest","message":"Bad request"})"));
    }

private:
    static json::object SerializeMap(const model::Map& map) {
        json::array roads;
        for (const auto& road : map.GetRoads()) {
            const auto start = road.GetStart();
            if (road.IsHorizontal()) {
                roads.emplace_back(json::object{
                    {"x0", start.x},
                    {"y0", start.y},
                    {"x1", road.GetEnd().x},
                });
            } else {
                roads.emplace_back(json::object{
                    {"x0", start.x},
                    {"y0", start.y},
                    {"y1", road.GetEnd().y},
                });
            }
        }

        json::array buildings;
        for (const auto& building : map.GetBuildings()) {
            const auto& bounds = building.GetBounds();
            buildings.emplace_back(json::object{
                {"x", bounds.position.x},
                {"y", bounds.position.y},
                {"w", bounds.size.width},
                {"h", bounds.size.height},
            });
        }

        json::array offices;
        for (const auto& office : map.GetOffices()) {
            const auto pos = office.GetPosition();
            const auto offset = office.GetOffset();
            offices.emplace_back(json::object{
                {"id", *office.GetId()},
                {"x", pos.x},
                {"y", pos.y},
                {"offsetX", offset.dx},
                {"offsetY", offset.dy},
            });
        }

        return json::object{
            {"id", *map.GetId()},
            {"name", map.GetName()},
            {"roads", std::move(roads)},
            {"buildings", std::move(buildings)},
            {"offices", std::move(offices)},
        };
    }

    model::Game& game_;
};

}  // namespace http_handler
