#pragma once

#include "model.h"

#include <boost/json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace game_serialization {

namespace json = boost::json;
namespace fs = std::filesystem;

inline double AsDouble(const json::value& value) {
    if (value.is_double()) {
        return value.as_double();
    }
    if (value.is_int64()) {
        return static_cast<double>(value.as_int64());
    }
    if (value.is_uint64()) {
        return static_cast<double>(value.as_uint64());
    }
    throw std::runtime_error("Expected number");
}

inline int AsInt(const json::value& value) {
    if (value.is_int64()) {
        return static_cast<int>(value.as_int64());
    }
    if (value.is_uint64()) {
        return static_cast<int>(value.as_uint64());
    }
    throw std::runtime_error("Expected integer");
}

inline size_t AsSize(const json::value& value) {
    if (value.is_uint64()) {
        return static_cast<size_t>(value.as_uint64());
    }
    if (value.is_int64()) {
        return static_cast<size_t>(value.as_int64());
    }
    throw std::runtime_error("Expected size");
}

inline json::value SerializeGame(model::Game& game) {
    json::array sessions;
    for (auto& map : game.GetMaps()) {
        auto* session = game.FindSession(map.GetId());
        if (!session) {
            continue;
        }
        json::object session_obj;
        session_obj["mapId"] = *map.GetId();

        json::array players;
        for (const auto& player : session->GetPlayers()) {
            const auto* dog = player.GetDog();
            json::object player_obj;
            player_obj["token"] = player.GetToken();
            player_obj["name"] = dog->GetName();
            player_obj["pos"] = json::array{dog->GetPosition().first, dog->GetPosition().second};
            player_obj["speed"] = json::array{dog->GetSpeed().first, dog->GetSpeed().second};
            player_obj["dir"] = dog->GetDirection();
            player_obj["score"] = player.GetScore();
            player_obj["idleMs"] = player.GetIdleTimeMs();
            player_obj["playMs"] = player.GetPlayTimeMs();
            json::array bag;
            for (const auto& item : player.GetBag()) {
                bag.push_back(json::object{{"id", item.id}, {"type", item.type}});
            }
            player_obj["bag"] = std::move(bag);
            players.push_back(std::move(player_obj));
        }
        session_obj["players"] = std::move(players);

        json::array lost_objects;
        for (const auto& object : session->GetLostObjects()) {
            lost_objects.push_back(json::object{
                {"id", object.id},
                {"type", object.type},
                {"pos", json::array{object.position.first, object.position.second}},
            });
        }
        session_obj["lostObjects"] = std::move(lost_objects);
        session_obj["nextLostObjectId"] = session->GetNextLostObjectId();
        sessions.push_back(std::move(session_obj));
    }
    return json::object{{"sessions", std::move(sessions)}};
}

inline void DeserializeGame(model::Game& game, const json::value& value) {
    game.ResetDynamicState();
    const auto& root = value.as_object();
    for (const auto& session_value : root.at("sessions").as_array()) {
        const auto& session_obj = session_value.as_object();
        model::Map::Id map_id{std::string(session_obj.at("mapId").as_string())};
        auto* map = game.FindMap(map_id);
        if (!map) {
            throw std::runtime_error("Unknown map in saved state");
        }
        auto& session = game.GetOrCreateSession(map_id, map);

        for (const auto& player_value : session_obj.at("players").as_array()) {
            const auto& player_obj = player_value.as_object();
            const std::string name(player_obj.at("name").as_string());
            const std::string token(player_obj.at("token").as_string());
            auto* player = session.AddPlayer(name, token, false);
            auto* dog = player->GetDog();
            const auto& pos = player_obj.at("pos").as_array();
            const auto& speed = player_obj.at("speed").as_array();
            dog->SetPosition(AsDouble(pos.at(0)), AsDouble(pos.at(1)));
            dog->SetSpeed(AsDouble(speed.at(0)), AsDouble(speed.at(1)));
            dog->SetDirection(std::string(player_obj.at("dir").as_string()));
            player->SetScore(AsInt(player_obj.at("score")));
            player->SetIdleTimeMs(AsInt(player_obj.at("idleMs")));
            player->SetPlayTimeMs(AsInt(player_obj.at("playMs")));
            std::vector<model::BagItem> bag;
            for (const auto& item_value : player_obj.at("bag").as_array()) {
                const auto& item_obj = item_value.as_object();
                bag.push_back(model::BagItem{
                    .id = AsSize(item_obj.at("id")),
                    .type = AsSize(item_obj.at("type")),
                });
            }
            player->SetBag(std::move(bag));
        }

        std::vector<model::LostObject> lost_objects;
        for (const auto& object_value : session_obj.at("lostObjects").as_array()) {
            const auto& object_obj = object_value.as_object();
            const auto& pos = object_obj.at("pos").as_array();
            lost_objects.push_back(model::LostObject{
                .id = AsSize(object_obj.at("id")),
                .type = AsSize(object_obj.at("type")),
                .position = {AsDouble(pos.at(0)), AsDouble(pos.at(1))},
            });
        }
        session.SetLostObjects(std::move(lost_objects), AsSize(session_obj.at("nextLostObjectId")));
    }
}

inline void SaveGameToFile(model::Game& game, const fs::path& path) {
    const auto tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Failed to open temporary state file");
        }
        out << json::serialize(SerializeGame(game));
        if (!out) {
            throw std::runtime_error("Failed to write temporary state file");
        }
    }
    fs::rename(tmp, path);
}

inline void LoadGameFromFile(model::Game& game, const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open state file");
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    DeserializeGame(game, json::parse(content));
}

}  // namespace game_serialization
