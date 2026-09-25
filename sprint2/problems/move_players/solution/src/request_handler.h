#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <variant>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace net = boost::asio;
using tcp = net::ip::tcp;

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
inline constexpr const char* code = "code";
inline constexpr const char* message = "message";
inline constexpr const char* auth_token = "authToken";
inline constexpr const char* player_id = "playerId";
inline constexpr const char* user_name = "userName";
inline constexpr const char* map_id = "mapId";
inline constexpr const char* pos = "pos";
inline constexpr const char* speed = "speed";
inline constexpr const char* dir = "dir";
inline constexpr const char* players = "players";
inline constexpr const char* move = "move";
inline constexpr const char* time_delta = "timeDelta";
}  // namespace json_keys

inline constexpr std::string_view maps_target = "/api/v1/maps";
inline constexpr std::string_view maps_prefix = "/api/v1/maps/";
inline constexpr std::string_view join_target = "/api/v1/game/join";
inline constexpr std::string_view players_target = "/api/v1/game/players";
inline constexpr std::string_view state_target = "/api/v1/game/state";
inline constexpr std::string_view action_target = "/api/v1/game/player/action";
inline constexpr std::string_view tick_target = "/api/v1/game/tick";
inline constexpr std::string_view api_prefix = "/api/";

inline constexpr std::string_view content_type_json = "application/json";
inline constexpr std::string_view bad_request_body = R"({"code":"badRequest","message":"Bad request"})";
inline constexpr std::string_view map_not_found_body = R"({"code":"mapNotFound","message":"Map not found"})";

inline std::string UrlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex(str[i + 1]);
            int lo = hex(str[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result += static_cast<char>(hi * 16 + lo);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

inline std::string_view GetMimeType(std::string_view ext) {
    std::string e;
    e.reserve(ext.size());
    for (char c : ext) {
        e += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (e == ".htm" || e == ".html") return "text/html";
    if (e == ".css") return "text/css";
    if (e == ".txt") return "text/plain";
    if (e == ".js") return "text/javascript";
    if (e == ".json") return "application/json";
    if (e == ".xml") return "application/xml";
    if (e == ".png") return "image/png";
    if (e == ".jpg" || e == ".jpe" || e == ".jpeg") return "image/jpeg";
    if (e == ".gif") return "image/gif";
    if (e == ".bmp") return "image/bmp";
    if (e == ".ico") return "image/vnd.microsoft.icon";
    if (e == ".tiff" || e == ".tif") return "image/tiff";
    if (e == ".svg" || e == ".svgz") return "image/svg+xml";
    if (e == ".mp3") return "audio/mpeg";
    return "application/octet-stream";
}

inline bool IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);
    auto [b_end, mismatch] = std::mismatch(base.begin(), base.end(), path.begin(), path.end());
    return b_end == base.end();
}

inline std::optional<std::string> ExtractBearerToken(std::string_view auth) {
    constexpr std::string_view prefix = "Bearer ";
    if (!auth.starts_with(prefix)) {
        return std::nullopt;
    }
    auto token = auth.substr(prefix.size());
    if (token.size() != 32) {
        return std::nullopt;
    }
    for (unsigned char c : token) {
        if (!std::isxdigit(c)) {
            return std::nullopt;
        }
    }
    return std::string(token);
}

inline std::string JsonError(const std::string& c, const std::string& m) {
    json::object o;
    o[json_keys::code] = c;
    o[json_keys::message] = m;
    return json::serialize(o);
}

class RequestHandler {
public:
    RequestHandler(model::Game& game, const fs::path& static_root, bool auto_tick = false)
        : game_{game}, static_root_{fs::absolute(static_root)}, auto_tick_{auto_tick} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::literals;
        const bool is_head = req.method() == http::verb::head;
        std::string_view target = req.target();
        if (auto pos = target.find('?'); pos != std::string_view::npos) {
            target = target.substr(0, pos);
        }

        auto make_json = [&req, is_head](http::status s, std::string body,
                                          std::optional<std::string_view> cache = std::nullopt,
                                          std::optional<std::string_view> allow = std::nullopt) {
            http::response<http::string_body> r(s, req.version());
            r.set(http::field::content_type, content_type_json);
            if (cache) {
                r.set(http::field::cache_control, *cache);
            }
            if (allow) {
                r.set(http::field::allow, *allow);
            }
            r.keep_alive(req.keep_alive());
            r.body() = std::move(body);
            r.content_length(r.body().size());
            if (is_head) {
                r.body().clear();
            }
            return r;
        };

        if (target.starts_with(api_prefix)) {
            HandleApi(req, send, target, make_json);
            return;
        }
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send(make_json(http::status::bad_request, std::string{bad_request_body}));
            return;
        }
        ServeStatic(req, std::forward<Send>(send));
    }

private:
    template <typename R, typename S, typename J>
    void HandleApi(R& req, S& send, std::string_view t, J make_json) {
        using namespace std::literals;
        if (t == maps_target) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                send(make_json(http::status::bad_request, std::string{bad_request_body}));
                return;
            }
            json::array a;
            for (auto& m : game_.GetMaps()) {
                a.emplace_back(json::object{{json_keys::id, *m.GetId()}, {json_keys::name, m.GetName()}});
            }
            send(make_json(http::status::ok, json::serialize(a)));
            return;
        }
        if (t.starts_with(maps_prefix)) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                send(make_json(http::status::bad_request, std::string{bad_request_body}));
                return;
            }
            std::string mid{t.substr(maps_prefix.size())};
            if (mid.empty() || mid.find('/') != std::string::npos) {
                send(make_json(http::status::bad_request, std::string{bad_request_body}));
                return;
            }
            auto* m = game_.FindMap(model::Map::Id(mid));
            if (!m) {
                send(make_json(http::status::not_found, std::string{map_not_found_body}));
                return;
            }
            send(make_json(http::status::ok, json::serialize(SerializeMap(*m))));
            return;
        }
        if (t == join_target) {
            HandleJoin(req, send, make_json);
            return;
        }
        if (t == players_target) {
            HandlePlayers(req, send, make_json);
            return;
        }
        if (t == state_target) {
            HandleState(req, send, make_json);
            return;
        }
        if (t == action_target) {
            HandleAction(req, send, make_json);
            return;
        }
        if (t == tick_target) {
            HandleTick(req, send, make_json);
            return;
        }
        send(make_json(http::status::bad_request, std::string{bad_request_body}, "no-cache"sv));
    }

    template <typename R, typename S, typename J>
    void HandleJoin(R& req, S& send, J make_json) {
        using namespace std::literals;
        if (req.method() != http::verb::post) {
            send(make_json(http::status::method_not_allowed,
                           JsonError("invalidMethod", "Only POST method is expected"),
                           "no-cache"sv, "POST"sv));
            return;
        }
        auto ct = req.find(http::field::content_type);
        if (ct == req.end() || ct->value() != "application/json") {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Invalid content type"), "no-cache"sv));
            return;
        }
        json::value body;
        try {
            body = json::parse(req.body());
        } catch (...) {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Join game request parse error"), "no-cache"sv));
            return;
        }
        if (!body.is_object()) {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Join game request parse error"), "no-cache"sv));
            return;
        }
        auto& o = body.as_object();
        if (!o.contains(json_keys::user_name) || !o.contains(json_keys::map_id) ||
            !o.at(json_keys::user_name).is_string() || !o.at(json_keys::map_id).is_string()) {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Join game request parse error"), "no-cache"sv));
            return;
        }
        std::string un = std::string(o.at(json_keys::user_name).as_string());
        std::string mi = std::string(o.at(json_keys::map_id).as_string());
        if (un.empty()) {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Invalid name"), "no-cache"sv));
            return;
        }
        model::Map::Id mid(mi);
        auto* m = game_.FindMap(mid);
        if (!m) {
            send(make_json(http::status::not_found,
                           JsonError("mapNotFound", "Map not found"), "no-cache"sv));
            return;
        }
        auto& session = game_.GetOrCreateSession(mid, m);
        std::string tok = model::Game::GenerateToken();
        auto* p = session.AddPlayer(un, tok, game_.GetRandomizeSpawnPoints());
        const auto& ps = session.GetPlayers();
        uint32_t pid = 0;
        for (size_t i = 0; i < ps.size(); ++i) {
            if (&ps[i] == p) {
                pid = static_cast<uint32_t>(i);
                break;
            }
        }
        json::object rb;
        rb[json_keys::auth_token] = tok;
        rb[json_keys::player_id] = pid;
        send(make_json(http::status::ok, json::serialize(rb), "no-cache"sv));
    }

    template <typename R, typename S, typename J>
    void HandlePlayers(R& req, S& send, J make_json) {
        using namespace std::literals;
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send(make_json(http::status::method_not_allowed,
                           JsonError("invalidMethod", "Invalid method"),
                           "no-cache"sv, "GET, HEAD"sv));
            return;
        }
        auto ai = req.find(http::field::authorization);
        if (ai == req.end()) {
            send(make_json(http::status::unauthorized,
                           JsonError("invalidToken", "Authorization header is missing"), "no-cache"sv));
            return;
        }
        auto to = ExtractBearerToken(ai->value());
        if (!to) {
            send(make_json(http::status::unauthorized,
                           JsonError("invalidToken", "Authorization header is missing"), "no-cache"sv));
            return;
        }
        model::Player* pl = nullptr;
        model::GameSession* gs = nullptr;
        for (auto& m : game_.GetMaps()) {
            if (auto* s = game_.FindSession(m.GetId())) {
                if (auto* p = s->FindPlayerByToken(*to)) {
                    pl = p;
                    gs = s;
                    break;
                }
            }
        }
        if (!pl) {
            send(make_json(http::status::unauthorized,
                           JsonError("unknownToken", "Player token has not been found"), "no-cache"sv));
            return;
        }
        json::object po;
        for (size_t i = 0; i < gs->GetPlayers().size(); ++i) {
            json::object pi;
            pi[json_keys::name] = gs->GetPlayers()[i].GetDog()->GetName();
            po[std::to_string(i)] = pi;
        }
        send(make_json(http::status::ok, json::serialize(po), "no-cache"sv));
    }

    template <typename R, typename S, typename J>
    void HandleState(R& req, S& send, J make_json) {
        using namespace std::literals;
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send(make_json(http::status::method_not_allowed,
                           JsonError("invalidMethod", "Invalid method"),
                           "no-cache"sv, "GET, HEAD"sv));
            return;
        }
        auto ai = req.find(http::field::authorization);
        if (ai == req.end()) {
            send(make_json(http::status::unauthorized,
                           JsonError("invalidToken", "Authorization header is required"), "no-cache"sv));
            return;
        }
        auto to = ExtractBearerToken(ai->value());
        if (!to) {
            send(make_json(http::status::unauthorized,
                           JsonError("invalidToken", "Authorization header is required"), "no-cache"sv));
            return;
        }
        model::Player* pl = nullptr;
        model::GameSession* gs = nullptr;
        for (auto& m : game_.GetMaps()) {
            if (auto* s = game_.FindSession(m.GetId())) {
                if (auto* p = s->FindPlayerByToken(*to)) {
                    pl = p;
                    gs = s;
                    break;
                }
            }
        }
        if (!pl) {
            send(make_json(http::status::unauthorized,
                           JsonError("unknownToken", "Player token has not been found"), "no-cache"sv));
            return;
        }
        json::object po;
        for (size_t i = 0; i < gs->GetPlayers().size(); ++i) {
            auto& d = *gs->GetPlayers()[i].GetDog();
            json::object di;
            json::array pa;
            pa.emplace_back(d.GetPosition().first);
            pa.emplace_back(d.GetPosition().second);
            di[json_keys::pos] = pa;
            json::array sa;
            sa.emplace_back(d.GetSpeed().first);
            sa.emplace_back(d.GetSpeed().second);
            di[json_keys::speed] = sa;
            di[json_keys::dir] = d.GetDirection();
            po[std::to_string(i)] = di;
        }
        json::object rb;
        rb[json_keys::players] = po;
        send(make_json(http::status::ok, json::serialize(rb), "no-cache"sv));
    }

    template <typename R, typename S, typename J>
    void HandleAction(R& req, S& send, J make_json) {
        using namespace std::literals;
        if (req.method() != http::verb::post) {
            send(make_json(http::status::method_not_allowed,
                           JsonError("invalidMethod", "Invalid method"),
                           "no-cache"sv, "POST"sv));
            return;
        }
        auto ct = req.find(http::field::content_type);
        if (ct == req.end() || ct->value() != "application/json") {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Invalid content type"), "no-cache"sv));
            return;
        }
        auto ai = req.find(http::field::authorization);
        if (ai == req.end()) {
            send(make_json(http::status::unauthorized,
                           JsonError("invalidToken", "Authorization header is required"), "no-cache"sv));
            return;
        }
        auto to = ExtractBearerToken(ai->value());
        if (!to) {
            send(make_json(http::status::unauthorized,
                           JsonError("invalidToken", "Authorization header is required"), "no-cache"sv));
            return;
        }
        model::Player* pl = nullptr;
        model::GameSession* gs = nullptr;
        for (auto& m : game_.GetMaps()) {
            if (auto* s = game_.FindSession(m.GetId())) {
                if (auto* p = s->FindPlayerByToken(*to)) {
                    pl = p;
                    gs = s;
                    break;
                }
            }
        }
        if (!pl) {
            send(make_json(http::status::unauthorized,
                           JsonError("unknownToken", "Player token has not been found"), "no-cache"sv));
            return;
        }
        json::value body;
        try {
            body = json::parse(req.body());
        } catch (...) {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Failed to parse action"), "no-cache"sv));
            return;
        }
        if (!body.is_object() || !body.as_object().contains(json_keys::move) ||
            !body.as_object().at(json_keys::move).is_string()) {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Failed to parse action"), "no-cache"sv));
            return;
        }
        std::string mv = std::string(body.as_object().at(json_keys::move).as_string());
        double speed = gs->GetMap()->GetDogSpeed();
        auto* d = pl->GetDog();
        if (mv == "L") {
            d->SetSpeed(-speed, 0);
            d->SetDirection("L");
        } else if (mv == "R") {
            d->SetSpeed(speed, 0);
            d->SetDirection("R");
        } else if (mv == "U") {
            d->SetSpeed(0, -speed);
            d->SetDirection("U");
        } else if (mv == "D") {
            d->SetSpeed(0, speed);
            d->SetDirection("D");
        } else if (mv.empty()) {
            d->SetSpeed(0, 0);
        } else {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Failed to parse action"), "no-cache"sv));
            return;
        }
        send(make_json(http::status::ok, "{}", "no-cache"sv));
    }

    template <typename R, typename S, typename J>
    void HandleTick(R& req, S& send, J make_json) {
        using namespace std::literals;
        if (auto_tick_) {
            send(make_json(http::status::bad_request,
                           JsonError("badRequest", "Invalid endpoint"), "no-cache"sv));
            return;
        }
        if (req.method() != http::verb::post) {
            send(make_json(http::status::method_not_allowed,
                           JsonError("invalidMethod", "Invalid method"),
                           "no-cache"sv, "POST"sv));
            return;
        }
        json::value body;
        try {
            body = json::parse(req.body());
        } catch (...) {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Failed to parse tick request JSON"), "no-cache"sv));
            return;
        }
        if (!body.is_object() || !body.as_object().contains(json_keys::time_delta) ||
            !body.as_object().at(json_keys::time_delta).is_int64()) {
            send(make_json(http::status::bad_request,
                           JsonError("invalidArgument", "Failed to parse tick request JSON"), "no-cache"sv));
            return;
        }
        int td = static_cast<int>(body.as_object().at(json_keys::time_delta).as_int64());
        for (auto& m : game_.GetMaps()) {
            if (auto* s = game_.FindSession(m.GetId())) {
                s->Tick(td);
            }
        }
        send(make_json(http::status::ok, "{}", "no-cache"sv));
    }

    static json::object SerializeMap(const model::Map& map) {
        json::array roads;
        for (auto& r : map.GetRoads()) {
            auto s = r.GetStart();
            if (r.IsHorizontal()) {
                roads.emplace_back(json::object{{json_keys::x0, s.x}, {json_keys::y0, s.y}, {json_keys::x1, r.GetEnd().x}});
            } else {
                roads.emplace_back(json::object{{json_keys::x0, s.x}, {json_keys::y0, s.y}, {json_keys::y1, r.GetEnd().y}});
            }
        }
        json::array bld;
        for (auto& b : map.GetBuildings()) {
            auto& bb = b.GetBounds();
            bld.emplace_back(json::object{{json_keys::x, bb.position.x},
                                          {json_keys::y, bb.position.y},
                                          {json_keys::w, bb.size.width},
                                          {json_keys::h, bb.size.height}});
        }
        json::array off;
        for (auto& o : map.GetOffices()) {
            auto p = o.GetPosition();
            auto offs = o.GetOffset();
            off.emplace_back(json::object{{json_keys::id, *o.GetId()},
                                          {json_keys::x, p.x},
                                          {json_keys::y, p.y},
                                          {json_keys::offset_x, offs.dx},
                                          {json_keys::offset_y, offs.dy}});
        }
        return json::object{{json_keys::id, *map.GetId()},
                            {json_keys::name, map.GetName()},
                            {json_keys::roads, std::move(roads)},
                            {json_keys::buildings, std::move(bld)},
                            {json_keys::offices, std::move(off)}};
    }

    template <typename S>
    void ServeStatic(const auto& req, S&& send) {
        using namespace std::literals;
        const bool is_head = req.method() == http::verb::head;
        std::string dt = UrlDecode(std::string(req.target()));
        if (auto p = dt.find('?'); p != std::string::npos) {
            dt = dt.substr(0, p);
        }
        if (!dt.empty() && dt[0] == '/') {
            dt = dt.substr(1);
        }
        fs::path fp = static_root_ / fs::path(dt);
        std::error_code ec;
        fs::path cp = fs::weakly_canonical(fp, ec);
        if (ec || !IsSubPath(cp, static_root_)) {
            auto r = MakeTextResp(req, http::status::bad_request, "Bad request"s);
            if (is_head) r.body().clear();
            send(std::move(r));
            return;
        }
        if (fs::is_directory(cp)) {
            cp /= "index.html";
        }
        if (!fs::exists(cp) || !fs::is_regular_file(cp)) {
            auto r = MakeTextResp(req, http::status::not_found, "File not found"s);
            if (is_head) r.body().clear();
            send(std::move(r));
            return;
        }
        std::ifstream f(cp, std::ios::binary);
        if (!f) {
            auto r = MakeTextResp(req, http::status::not_found, "File not found"s);
            if (is_head) r.body().clear();
            send(std::move(r));
            return;
        }
        std::string content{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
        auto mt = GetMimeType(cp.extension().string());
        http::response<http::string_body> r(http::status::ok, req.version());
        r.set(http::field::content_type, mt);
        r.body() = std::move(content);
        r.content_length(r.body().size());
        if (is_head) {
            r.body().clear();
        }
        r.keep_alive(req.keep_alive());
        send(std::move(r));
    }

    template <typename Req>
    static http::response<http::string_body> MakeTextResp(const Req& req, http::status st, std::string b) {
        http::response<http::string_body> r(st, req.version());
        r.set(http::field::content_type, "text/plain");
        r.body() = std::move(b);
        r.content_length(r.body().size());
        r.keep_alive(req.keep_alive());
        return r;
    }

    model::Game& game_;
    fs::path static_root_;
    bool auto_tick_;
};

}  // namespace http_handler
