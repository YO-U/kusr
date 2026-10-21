#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <string_view>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

// Ключи JSON-объектов при сериализации ответов.
// Тип const char* выбран намеренно: boost::json 1.78 принимает ключи как
// boost::json::string_view (boost::string_view), который не конструируется
// из std::string_view, а из const char* — да.
namespace json_keys {
inline constexpr const char* id = "id";
inline constexpr const char* name = "name";
inline constexpr const char* x = "x";
inline constexpr const char* y = "y";
inline constexpr const char* w = "w";
inline constexpr const char* h = "h";
inline constexpr const char* x0 = "x0";
inline constexpr const char* y0 = "y0";
inline constexpr const char* x1 = "x1";
inline constexpr const char* y1 = "y1";
inline constexpr const char* offset_x = "offsetX";
inline constexpr const char* offset_y = "offsetY";
inline constexpr const char* roads = "roads";
inline constexpr const char* buildings = "buildings";
inline constexpr const char* offices = "offices";
}  // namespace json_keys

// Пути HTTP API.
inline constexpr std::string_view maps_target = "/api/v1/maps";
inline constexpr std::string_view maps_prefix = "/api/v1/maps/";
inline constexpr std::string_view api_prefix = "/api/";

// Общие строки для формирования ответов.
inline constexpr std::string_view content_type_json = "application/json";
inline constexpr std::string_view bad_request_body =
    R"({"code":"badRequest","message":"Bad request"})";
inline constexpr std::string_view map_not_found_body =
    R"({"code":"mapNotFound","message":"Map not found"})";

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
            response.set(http::field::content_type, content_type_json);
            response.keep_alive(req.keep_alive());
            response.body() = std::move(body);
            response.content_length(response.body().size());
            return response;
        };

        if (req.method() != http::verb::get) {
            send(json_response(http::status::bad_request, std::string{bad_request_body}));
            return;
        }

        if (target == maps_target) {
            json::array maps;
            for (const auto& map : game_.GetMaps()) {
                maps.emplace_back(json::object{
                    {json_keys::id, *map.GetId()},
                    {json_keys::name, map.GetName()},
                });
            }
            send(json_response(http::status::ok, json::serialize(maps)));
            return;
        }

        if (target.starts_with(maps_prefix)) {
            const std::string map_id{target.substr(maps_prefix.size())};
            if (map_id.empty() || map_id.find('/') != std::string::npos) {
                send(json_response(http::status::bad_request, std::string{bad_request_body}));
                return;
            }

            const model::Map* map = game_.FindMap(model::Map::Id(map_id));
            if (!map) {
                send(json_response(http::status::not_found, std::string{map_not_found_body}));
                return;
            }

            send(json_response(http::status::ok, json::serialize(SerializeMap(*map))));
            return;
        }

        if (target.starts_with(api_prefix)) {
            send(json_response(http::status::bad_request, std::string{bad_request_body}));
            return;
        }

        send(json_response(http::status::bad_request, std::string{bad_request_body}));
    }

private:
    static json::object SerializeMap(const model::Map& map) {
        json::array roads;
        for (const auto& road : map.GetRoads()) {
            const auto start = road.GetStart();
            if (road.IsHorizontal()) {
                roads.emplace_back(json::object{
                    {json_keys::x0, start.x},
                    {json_keys::y0, start.y},
                    {json_keys::x1, road.GetEnd().x},
                });
            } else {
                roads.emplace_back(json::object{
                    {json_keys::x0, start.x},
                    {json_keys::y0, start.y},
                    {json_keys::y1, road.GetEnd().y},
                });
            }
        }

        json::array buildings;
        for (const auto& building : map.GetBuildings()) {
            const auto& bounds = building.GetBounds();
            buildings.emplace_back(json::object{
                {json_keys::x, bounds.position.x},
                {json_keys::y, bounds.position.y},
                {json_keys::w, bounds.size.width},
                {json_keys::h, bounds.size.height},
            });
        }

        json::array offices;
        for (const auto& office : map.GetOffices()) {
            const auto pos = office.GetPosition();
            const auto offset = office.GetOffset();
            offices.emplace_back(json::object{
                {json_keys::id, *office.GetId()},
                {json_keys::x, pos.x},
                {json_keys::y, pos.y},
                {json_keys::offset_x, offset.dx},
                {json_keys::offset_y, offset.dy},
            });
        }

        return json::object{
            {json_keys::id, *map.GetId()},
            {json_keys::name, map.GetName()},
            {json_keys::roads, std::move(roads)},
            {json_keys::buildings, std::move(buildings)},
            {json_keys::offices, std::move(offices)},
        };
    }

    model::Game& game_;
};

}  // namespace http_handler
