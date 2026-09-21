#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_loader {

namespace json = boost::json;

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream input(json_path);
    if (!input) {
        throw std::runtime_error("Failed to open config file");
    }

    std::ostringstream ss;
    ss << input.rdbuf();
    auto value = json::parse(ss.str());
    const auto& root = value.as_object();
    const auto& maps = root.at("maps").as_array();

    model::Game game;
    for (const auto& map_value : maps) {
        const auto& map_obj = map_value.as_object();
        model::Map map(
            model::Map::Id(std::string(map_obj.at("id").as_string())),
            std::string(map_obj.at("name").as_string()));

        for (const auto& road_value : map_obj.at("roads").as_array()) {
            const auto& road_obj = road_value.as_object();
            const int x0 = static_cast<int>(road_obj.at("x0").as_int64());
            const int y0 = static_cast<int>(road_obj.at("y0").as_int64());
            if (auto it = road_obj.find("x1"); it != road_obj.end()) {
                map.AddRoad(model::Road(
                    model::Road::HORIZONTAL,
                    model::Point{x0, y0},
                    static_cast<model::Coord>(it->value().as_int64())));
            } else {
                map.AddRoad(model::Road(
                    model::Road::VERTICAL,
                    model::Point{x0, y0},
                    static_cast<model::Coord>(road_obj.at("y1").as_int64())));
            }
        }

        if (auto buildings = map_obj.find("buildings"); buildings != map_obj.end()) {
            for (const auto& building_value : buildings->value().as_array()) {
                const auto& b = building_value.as_object();
                map.AddBuilding(model::Building(model::Rectangle{
                    model::Point{
                        static_cast<model::Coord>(b.at("x").as_int64()),
                        static_cast<model::Coord>(b.at("y").as_int64())},
                    model::Size{
                        static_cast<model::Dimension>(b.at("w").as_int64()),
                        static_cast<model::Dimension>(b.at("h").as_int64())}}));
            }
        }

        if (auto offices = map_obj.find("offices"); offices != map_obj.end()) {
            for (const auto& office_value : offices->value().as_array()) {
                const auto& o = office_value.as_object();
                map.AddOffice(model::Office(
                    model::Office::Id(std::string(o.at("id").as_string())),
                    model::Point{
                        static_cast<model::Coord>(o.at("x").as_int64()),
                        static_cast<model::Coord>(o.at("y").as_int64())},
                    model::Offset{
                        static_cast<model::Dimension>(o.at("offsetX").as_int64()),
                        static_cast<model::Dimension>(o.at("offsetY").as_int64())}));
            }
        }

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader
