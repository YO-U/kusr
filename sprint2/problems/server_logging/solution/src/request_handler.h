
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
#include <mutex>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

namespace http_handler {
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;

namespace json_keys {
inline constexpr const char* id = "id"; inline constexpr const char* name = "name";
inline constexpr const char* x = "x"; inline constexpr const char* y = "y";
inline constexpr const char* w = "w"; inline constexpr const char* h = "h";
inline constexpr const char* x0 = "x0"; inline constexpr const char* y0 = "y0";
inline constexpr const char* x1 = "x1"; inline constexpr const char* y1 = "y1";
inline constexpr const char* offset_x = "offsetX"; inline constexpr const char* offset_y = "offsetY";
inline constexpr const char* roads = "roads"; inline constexpr const char* buildings = "buildings";
inline constexpr const char* offices = "offices";
inline constexpr const char* code = "code"; inline constexpr const char* message = "message";
inline constexpr const char* auth_token = "authToken"; inline constexpr const char* player_id = "playerId";
inline constexpr const char* user_name = "userName"; inline constexpr const char* map_id = "mapId";
inline constexpr const char* pos = "pos"; inline constexpr const char* speed = "speed";
inline constexpr const char* dir = "dir"; inline constexpr const char* players = "players";
inline constexpr const char* move = "move"; inline constexpr const char* time_delta = "timeDelta";
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
    std::string result; result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == 0x25 && i + 2 < str.size()) {
            auto hex = [](char c)->int {
                if(c>=0x30&&c<=0x39)return c-0x30;
                if(c>=0x61&&c<=0x66)return c-0x61+10;
                if(c>=0x41&&c<=0x46)return c-0x41+10;
                return -1;
            };
            int hi=hex(str[i+1]),lo=hex(str[i+2]);
            if(hi>=0&&lo>=0){result+=static_cast<char>(hi*16+lo);i+=2;}
            else result+=str[i];
        }else if(str[i]==0x2B)result+=0x20;
        else result+=str[i];
    }
    return result;
}

inline std::string_view GetMimeType(std::string_view ext) {
    std::string e; e.reserve(ext.size());
    for(char c:ext)e+=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(e==".htm"||e==".html")return"text/html";
    if(e==".css")return"text/css";if(e==".txt")return"text/plain";
    if(e==".js")return"text/javascript";if(e==".json")return"application/json";
    if(e==".xml")return"application/xml";if(e==".png")return"image/png";
    if(e==".jpg"||e==".jpe"||e==".jpeg")return"image/jpeg";
    if(e==".gif")return"image/gif";if(e==".bmp")return"image/bmp";
    if(e==".ico")return"image/vnd.microsoft.icon";
    if(e==".tiff"||e==".tif")return"image/tiff";
    if(e==".svg"||e==".svgz")return"image/svg+xml";
    if(e==".mp3")return"audio/mpeg";
    return"application/octet-stream";
}

inline std::optional<std::string> ExtractBearerToken(std::string_view auth) {
    constexpr std::string_view p = "Bearer ";
    if(auth.starts_with(p)) return std::string(auth.substr(p.size()));
    return std::nullopt;
}

inline std::string JsonError(const std::string& c, const std::string& m) {
    json::object o; o[json_keys::code]=c; o[json_keys::message]=m;
    return json::serialize(o);
}

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

class RequestHandler {
public:
    RequestHandler(model::Game& game, const fs::path& static_root, bool auto_tick = false)
        : game_{game}, static_root_{static_root}, auto_tick_{auto_tick} {}
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, const std::string& client_ip = {}) {
        using namespace std::literals;
        const auto started = std::chrono::steady_clock::now();
        json::object req_data;
        req_data["ip"] = client_ip;
        req_data["URI"] = std::string(req.target());
        req_data["method"] = std::string(http::to_string(req.method()));
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, json::value(req_data)) << "request received"sv;
        auto logged_send = [&send, started, client_ip](auto&& response) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - started)
                                .count();
            json::object data;
            data["ip"] = client_ip;
            data["response_time"] = ms;
            data["code"] = response.result_int();
            if (auto it = response.find(http::field::content_type); it != response.end()) {
                data["content_type"] = std::string(it->value());
            } else {
                data["content_type"] = nullptr;
            }
            BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, json::value(data)) << "response sent"sv;
            send(std::forward<decltype(response)>(response));
        };
        std::string_view target = req.target();
        if(auto pos=target.find(0x3F);pos!=std::string_view::npos)target=target.substr(0,pos);
        auto jr = [&req](http::status s, std::string b, std::optional<std::string_view> cc=std::nullopt){
            http::response<http::string_body> r(s,req.version());
            r.set(http::field::content_type,content_type_json);
            if(cc) r.set(http::field::cache_control,*cc);
            r.keep_alive(req.keep_alive()); r.body()=std::move(b);
            r.content_length(r.body().size()); return r;
        };
        if(target.starts_with(api_prefix)){HandleApi(req,logged_send,target,jr);return;}
        if(req.method()!=http::verb::get&&req.method()!=http::verb::head){
            logged_send(jr(http::status::bad_request,std::string{bad_request_body}));return;
        }
        ServeStatic(req,logged_send);
    }

private:
    template<typename R,typename S,typename J>
    void HandleApi(R& req,S& send,std::string_view t,J jr){
        if(t==maps_target){
            if(req.method()!=http::verb::get&&req.method()!=http::verb::head){send(jr(http::status::bad_request,std::string{bad_request_body}));return;}
            json::array a; for(auto&m:game_.GetMaps())a.emplace_back(json::object{{json_keys::id,*m.GetId()},{json_keys::name,m.GetName()}});
            send(jr(http::status::ok,json::serialize(a))); return;
        }
        if(t.starts_with(maps_prefix)){
            if(req.method()!=http::verb::get&&req.method()!=http::verb::head){send(jr(http::status::bad_request,std::string{bad_request_body}));return;}
            std::string mid{t.substr(maps_prefix.size())};
            if(mid.empty()||mid.find(0x2F)!=std::string::npos){send(jr(http::status::bad_request,std::string{bad_request_body}));return;}
            auto* m=game_.FindMap(model::Map::Id(mid));
            if(!m){send(jr(http::status::not_found,std::string{map_not_found_body}));return;}
            send(jr(http::status::ok,json::serialize(SerializeMap(*m))));return;
        }
        if(t==join_target){HandleJoin(req,send,jr);return;}
        if(t==players_target){HandlePlayers(req,send,jr);return;}
        if(t==state_target){HandleState(req,send,jr);return;}
        if(t==action_target){HandleAction(req,send,jr);return;}
        if(t==tick_target){HandleTick(req,send,jr);return;}
        send(jr(http::status::bad_request,std::string{bad_request_body}));
    }

    template<typename R,typename S,typename J>
    void HandleJoin(R& req,S& send,J jr){
        if(req.method()!=http::verb::post){
            auto r=jr(http::status::method_not_allowed,JsonError("invalidMethod","Only POST method is expected"),"no-cache"sv);
            r.set(http::field::allow,"POST"); send(std::move(r)); return;
        }
        auto ct=req.find(http::field::content_type);
        if(ct==req.end()||ct->value()!="application/json"){send(jr(http::status::bad_request,JsonError("invalidArgument","Invalid content type"),"no-cache"sv));return;}
        json::value body;
        try{body=json::parse(req.body());}catch(...){send(jr(http::status::bad_request,JsonError("invalidArgument","Join game request parse error"),"no-cache"sv));return;}
        auto&o=body.as_object();
        if(!o.contains(json_keys::user_name)||!o.contains(json_keys::map_id)){send(jr(http::status::bad_request,JsonError("invalidArgument","Join game request parse error"),"no-cache"sv));return;}
        std::string un=std::string(o.at(json_keys::user_name).as_string());
        std::string mi=std::string(o.at(json_keys::map_id).as_string());
        if(un.empty()){send(jr(http::status::bad_request,JsonError("invalidArgument","Invalid name"),"no-cache"sv));return;}
        model::Map::Id mid(mi); auto* m=game_.FindMap(mid);
        if(!m){send(jr(http::status::not_found,JsonError("mapNotFound","Map not found"),"no-cache"sv));return;}
        auto&s=game_.GetOrCreateSession(mid,m);
        std::string tok=model::Game::GenerateToken();
        auto* p=s.AddPlayer(un,tok);
        const auto&ps=s.GetPlayers(); uint32_t pid=0;
        for(size_t i=0;i<ps.size();++i)if(&ps[i]==p){pid=static_cast<uint32_t>(i);break;}
        json::object rb; rb[json_keys::auth_token]=tok; rb[json_keys::player_id]=pid;
        send(jr(http::status::ok,json::serialize(rb),"no-cache"sv));
    }

    template<typename R,typename S,typename J>
    void HandlePlayers(R& req,S& send,J jr){
        if(req.method()!=http::verb::get&&req.method()!=http::verb::head){
            auto r=jr(http::status::method_not_allowed,JsonError("invalidMethod","Invalid method"),"no-cache"sv);
            r.set(http::field::allow,"GET, HEAD");send(std::move(r));return;
        }
        auto ai=req.find(http::field::authorization);
        if(ai==req.end()){send(jr(http::status::unauthorized,JsonError("invalidToken","Authorization header is missing"),"no-cache"sv));return;}
        auto to=ExtractBearerToken(ai->value());
        if(!to){send(jr(http::status::unauthorized,JsonError("invalidToken","Authorization header is missing"),"no-cache"sv));return;}
        model::Player* pl=nullptr; model::GameSession* gs=nullptr;
        for(auto&m:game_.GetMaps()){auto*s=game_.FindSession(m.GetId());if(s){auto*p=s->FindPlayerByToken(*to);if(p){pl=p;gs=s;break;}}}
        if(!pl){send(jr(http::status::unauthorized,JsonError("unknownToken","Player token has not been found"),"no-cache"sv));return;}
        json::object po;
        for(size_t i=0;i<gs->GetPlayers().size();++i){json::object pi; pi[json_keys::name]=gs->GetPlayers()[i].GetDog()->GetName(); po[std::to_string(i)]=pi;}
        send(jr(http::status::ok,json::serialize(po),"no-cache"sv));
    }

    template<typename R,typename S,typename J>
    void HandleState(R& req,S& send,J jr){
        if(req.method()!=http::verb::get&&req.method()!=http::verb::head){
            auto r=jr(http::status::method_not_allowed,JsonError("invalidMethod","Invalid method"),"no-cache"sv);
            r.set(http::field::allow,"GET, HEAD");send(std::move(r));return;
        }
        auto ai=req.find(http::field::authorization);
        if(ai==req.end()){send(jr(http::status::unauthorized,JsonError("invalidToken","Authorization header is required"),"no-cache"sv));return;}
        auto to=ExtractBearerToken(ai->value());
        if(!to){send(jr(http::status::unauthorized,JsonError("invalidToken","Authorization header is required"),"no-cache"sv));return;}
        model::Player* pl=nullptr; model::GameSession* gs=nullptr;
        for(auto&m:game_.GetMaps()){auto*s=game_.FindSession(m.GetId());if(s){auto*p=s->FindPlayerByToken(*to);if(p){pl=p;gs=s;break;}}}
        if(!pl){send(jr(http::status::unauthorized,JsonError("unknownToken","Player token has not been found"),"no-cache"sv));return;}
        json::object po;
        for(size_t i=0;i<gs->GetPlayers().size();++i){
            auto&d=*gs->GetPlayers()[i].GetDog();
            json::object di;
            json::array pa; pa.emplace_back(d.GetPosition().first); pa.emplace_back(d.GetPosition().second);
            di[json_keys::pos]=pa;
            json::array sa; sa.emplace_back(d.GetSpeed().first); sa.emplace_back(d.GetSpeed().second);
            di[json_keys::speed]=sa;
            di[json_keys::dir]=d.GetDirection();
            po[std::to_string(i)]=di;
        }
        json::object rb; rb[json_keys::players]=po;
        send(jr(http::status::ok,json::serialize(rb),"no-cache"sv));
    }

    template<typename R,typename S,typename J>
    void HandleAction(R& req,S& send,J jr){
        if(req.method()!=http::verb::post){
            auto r=jr(http::status::method_not_allowed,JsonError("invalidMethod","Invalid method"),"no-cache"sv);
            r.set(http::field::allow,"POST");send(std::move(r));return;
        }
        auto ct=req.find(http::field::content_type);
        if(ct==req.end()||ct->value()!="application/json"){send(jr(http::status::bad_request,JsonError("invalidArgument","Invalid content type"),"no-cache"sv));return;}
        auto ai=req.find(http::field::authorization);
        if(ai==req.end()){send(jr(http::status::unauthorized,JsonError("invalidToken","Authorization header is required"),"no-cache"sv));return;}
        auto to=ExtractBearerToken(ai->value());
        if(!to){send(jr(http::status::unauthorized,JsonError("invalidToken","Authorization header is required"),"no-cache"sv));return;}
        model::Player* pl=nullptr; model::GameSession* gs=nullptr;
        for(auto&m:game_.GetMaps()){auto*s=game_.FindSession(m.GetId());if(s){auto*p=s->FindPlayerByToken(*to);if(p){pl=p;gs=s;break;}}}
        if(!pl){send(jr(http::status::unauthorized,JsonError("unknownToken","Player token has not been found"),"no-cache"sv));return;}
        json::value body;
        try{body=json::parse(req.body());}catch(...){send(jr(http::status::bad_request,JsonError("invalidArgument","Failed to parse action"),"no-cache"sv));return;}
        auto&o=body.as_object();
        if(!o.contains(json_keys::move)){send(jr(http::status::bad_request,JsonError("invalidArgument","Failed to parse action"),"no-cache"sv));return;}
        std::string mv=std::string(o.at(json_keys::move).as_string());
        double s=gs->GetMap()->GetDogSpeed(); auto* d=pl->GetDog();
        if(mv=="L"){d->SetSpeed(-s,0);d->SetDirection("L");}
        else if(mv=="R"){d->SetSpeed(s,0);d->SetDirection("R");}
        else if(mv=="U"){d->SetSpeed(0,-s);d->SetDirection("U");}
        else if(mv=="D"){d->SetSpeed(0,s);d->SetDirection("D");}
        else if(mv==""){d->SetSpeed(0,0);}
        else{send(jr(http::status::bad_request,JsonError("invalidArgument","Failed to parse action"),"no-cache"sv));return;}
        send(jr(http::status::ok,"{}","no-cache"sv));
    }

    template<typename R,typename S,typename J>
    void HandleTick(R& req,S& send,J jr){
        if(auto_tick_){send(jr(http::status::bad_request,JsonError("badRequest","Invalid endpoint"),"no-cache"sv));return;}
        if(req.method()!=http::verb::post){send(jr(http::status::bad_request,JsonError("badRequest","Bad request"),"no-cache"sv));return;}
        json::value body;
        try{body=json::parse(req.body());}catch(...){send(jr(http::status::bad_request,JsonError("invalidArgument","Failed to parse tick request JSON"),"no-cache"sv));return;}
        auto&o=body.as_object();
        if(!o.contains(json_keys::time_delta)){send(jr(http::status::bad_request,JsonError("invalidArgument","Failed to parse tick request JSON"),"no-cache"sv));return;}
        int td=static_cast<int>(o.at(json_keys::time_delta).as_int64());
        for(auto&m:game_.GetMaps()){auto*s=game_.FindSession(m.GetId());if(s)s->Tick(td);}
        send(jr(http::status::ok,"{}","no-cache"sv));
    }

    static json::object SerializeMap(const model::Map& map){
        json::array roads;
        for(auto&r:map.GetRoads()){
            auto s=r.GetStart();
            if(r.IsHorizontal()) roads.emplace_back(json::object{{json_keys::x0,s.x},{json_keys::y0,s.y},{json_keys::x1,r.GetEnd().x}});
            else roads.emplace_back(json::object{{json_keys::x0,s.x},{json_keys::y0,s.y},{json_keys::y1,r.GetEnd().y}});
        }
        json::array bld;
        for(auto&b:map.GetBuildings()){auto&bb=b.GetBounds(); bld.emplace_back(json::object{{json_keys::x,bb.position.x},{json_keys::y,bb.position.y},{json_keys::w,bb.size.width},{json_keys::h,bb.size.height}});}
        json::array off;
        for(auto&o:map.GetOffices()){auto p=o.GetPosition();auto offs=o.GetOffset(); off.emplace_back(json::object{{json_keys::id,*o.GetId()},{json_keys::x,p.x},{json_keys::y,p.y},{json_keys::offset_x,offs.dx},{json_keys::offset_y,offs.dy}});}
        return json::object{{json_keys::id,*map.GetId()},{json_keys::name,map.GetName()},{json_keys::roads,std::move(roads)},{json_keys::buildings,std::move(bld)},{json_keys::offices,std::move(off)}};
    }

    template<typename S>
    void ServeStatic(const auto& req,S&& send){
        using namespace std::literals;
        std::string dt=UrlDecode(std::string(req.target()));
        if(auto p=dt.find(0x3F);p!=std::string::npos)dt=dt.substr(0,p);
        if(!dt.empty()&&dt[0]==0x2F)dt=dt.substr(1);
        fs::path rp(dt); fs::path fp=static_root_/rp;
        fs::path cr=fs::weakly_canonical(static_root_);
        fs::path cp=fs::weakly_canonical(fp);
        std::string crs=cr.generic_string();
        std::string cps=cp.generic_string();
        if(!crs.empty()&&crs.back()!='/') crs.push_back('/');
        if(cps!=cr.generic_string()&&!cps.starts_with(crs)){
            auto r=MakeResp(req,http::status::bad_request,"Bad request"s);
            r.set(http::field::content_type,"text/plain");r.content_length(r.body().size());send(std::move(r));return;
        }
        if(fs::is_directory(cp))cp/="index.html";
        if(!fs::exists(cp)||!fs::is_regular_file(cp)){
            auto r=MakeResp(req,http::status::not_found,"File not found"s);
            r.set(http::field::content_type,"text/plain");r.content_length(r.body().size());send(std::move(r));return;
        }
        std::ifstream f(cp,std::ios::binary);
        if(!f){auto r=MakeResp(req,http::status::not_found,"File not found"s);r.set(http::field::content_type,"text/plain");r.content_length(r.body().size());send(std::move(r));return;}
        std::string c{std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
        auto mt=GetMimeType(cp.extension().string());
        http::response<http::string_body> r(http::status::ok,req.version());
        r.set(http::field::content_type,mt);r.body()=std::move(c);
        r.content_length(r.body().size());r.keep_alive(req.keep_alive());send(std::move(r));
    }

    template<typename Req>
    static http::response<http::string_body> MakeResp(const Req& req,http::status st,std::string b){
        http::response<http::string_body> r(st,req.version());r.body()=std::move(b);
        r.keep_alive(req.keep_alive());return r;
    }

    model::Game& game_;
    fs::path static_root_;
    bool auto_tick_;
};

}  // namespace http_handler
