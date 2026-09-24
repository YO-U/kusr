#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace json_loader {

namespace json = boost::json;

namespace {

model::Road ParseRoad(const json::object& road_obj) {
    const int x0 = static_cast<int>(road_obj.at("x0").as_int64());
    const int y0 = static_cast<int>(road_obj.at("y0").as_int64());
    if (auto it = road_obj.find("x1"); it != road_obj.end()) {
        return model::Road(
            model::Road::HORIZONTAL,
            model::Point{x0, y0},
            static_cast<model::Coord>(it->value().as_int64()));
    }
    return model::Road(
        model::Road::VERTICAL,
        model::Point{x0, y0},
        static_cast<model::Coord>(road_obj.at("y1").as_int64()));
}

model::Building ParseBuilding(const json::object& building_obj) {
    return model::Building(model::Rectangle{
        model::Point{
            static_cast<model::Coord>(building_obj.at("x").as_int64()),
            static_cast<model::Coord>(building_obj.at("y").as_int64())},
        model::Size{
            static_cast<model::Dimension>(building_obj.at("w").as_int64()),
            static_cast<model::Dimension>(building_obj.at("h").as_int64())}});
}

model::Office ParseOffice(const json::object& office_obj) {
    return model::Office(
        model::Office::Id(std::string(office_obj.at("id").as_string())),
        model::Point{
            static_cast<model::Coord>(office_obj.at("x").as_int64()),
            static_cast<model::Coord>(office_obj.at("y").as_int64())},
        model::Offset{
            static_cast<model::Dimension>(office_obj.at("offsetX").as_int64()),
            static_cast<model::Dimension>(office_obj.at("offsetY").as_int64())});
}

void LoadRoads(const json::object& map_obj, model::Map& map) {
    for (const auto& road_value : map_obj.at("roads").as_array()) {
        map.AddRoad(ParseRoad(road_value.as_object()));
    }
}

void LoadBuildings(const json::object& map_obj, model::Map& map) {
    if (auto buildings = map_obj.find("buildings"); buildings != map_obj.end()) {
        for (const auto& building_value : buildings->value().as_array()) {
            map.AddBuilding(ParseBuilding(building_value.as_object()));
        }
    }
}

void LoadOffices(const json::object& map_obj, model::Map& map) {
    if (auto offices = map_obj.find("offices"); offices != map_obj.end()) {
        for (const auto& office_value : offices->value().as_array()) {
            map.AddOffice(ParseOffice(office_value.as_object()));
        }
    }
}

model::Map ParseMap(const json::value& map_value, double default_speed) {
    const auto& map_obj = map_value.as_object();
    model::Map map(
        model::Map::Id(std::string(map_obj.at("id").as_string())),
        std::string(map_obj.at("name").as_string()));

    LoadRoads(map_obj, map);
    LoadBuildings(map_obj, map);
    LoadOffices(map_obj, map);

    double speed = default_speed;
    if (auto ds = map_obj.find("dogSpeed"); ds != map_obj.end()) {
        speed = ds->value().as_double();
    }
    map.SetDogSpeed(speed);

    return map;
}

json::value ParseConfig(const std::string& text, const std::filesystem::path& json_path) {
    try {
        return json::parse(text);
    } catch (const std::exception& ex) {
        throw std::runtime_error("Failed to parse config file \"" + json_path.string() +
                                 "\": " + ex.what());
    }
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream input(json_path);
    if (!input) {
        throw std::runtime_error("Failed to open config file");
    }

    std::ostringstream ss;
    ss << input.rdbuf();
    const auto value = ParseConfig(ss.str(), json_path);
    const auto& root = value.as_object();

    model::Game game;
    double default_speed = 1.0;
    if (auto ds = root.find("defaultDogSpeed"); ds != root.end()) {
        default_speed = ds->value().as_double();
    }
    game.SetDefaultDogSpeed(default_speed);

    const auto& maps = root.at("maps").as_array();
    for (const auto& map_value : maps) {
        game.AddMap(ParseMap(map_value, default_speed));
    }

    return game;
}

}  // namespace json_loader