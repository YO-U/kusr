#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};
class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };
    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}, end_{end_x, start.y} {}
    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}, end_{start.x, end_y} {}

    bool IsHorizontal() const noexcept { return start_.y == end_.y; }
    bool IsVertical() const noexcept { return start_.x == end_.x; }
    Point GetStart() const noexcept { return start_; }
    Point GetEnd() const noexcept { return end_; }

private:
    Point start_, end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept : bounds_{bounds} {}
    const Rectangle& GetBounds() const noexcept { return bounds_; }
private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;
    Office(Id id, Point position, Offset offset) noexcept
class Dog {
public:
    Dog() : pos_{0.0, 0.0}, speed_{0.0, 0.0}, dir_("U") {}

    void SetPosition(double x, double y) {
        pos_.first = x;
        pos_.second = y;
    }
    void SetSpeed(double dx, double dy) {
        speed_.first = dx;
        speed_.second = dy;
    }
    void SetDirection(const std::string& dir) { dir_ = dir; }
    void SetName(const std::string& name) { name_ = name; }

    const std::pair<double, double>& GetPosition() const { return pos_; }
    const std::pair<double, double>& GetSpeed() const { return speed_; }
    const std::string& GetDirection() const { return dir_; }
    const std::string& GetName() const { return name_; }

private:
    std::string name_;
    std::pair<double, double> pos_;
class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id)), name_(std::move(name)) {}

    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    const Buildings& GetBuildings() const noexcept { return buildings_; }
    const Roads& GetRoads() const noexcept { return roads_; }
    const Offices& GetOffices() const noexcept { return offices_; }

    void AddRoad(const Road& road) { roads_.emplace_back(road); }
    void AddBuilding(const Building& building) { buildings_.emplace_back(building); }
    void AddOffice(Office office);

    const Dog* AddDog(const std::string& name) {
        dogs_.emplace_back();
        Dog& dog = dogs_.back();
        dog.SetName(name);
        // Spawn at start of first road (for game_state task)
        if (!roads_.empty()) {
            const auto& road = roads_[0];
            const auto start = road.GetStart();
            dog.SetPosition(static_cast<double>(start.x), static_cast<double>(start.y));
        }
        dog.SetSpeed(0.0, 0.0);
        dog.SetDirection("U");
        return &dog;
    }
    const std::vector<Dog>& GetDogs() const { return dogs_; }
    std::vector<Dog>& GetDogs() { return dogs_; }

    void SetDogSpeed(double speed) { dog_speed_ = speed; }
    double GetDogSpeed() const { return dog_speed_; }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    std::vector<Dog> dogs_;
    double dog_speed_ = 1.0;
};

class Player {
public:
    Player(Dog* dog, const std::string& token) : dog_(dog), token_(token) {}

    Dog* GetDog() { return dog_; }
    const Dog* GetDog() const { return dog_; }
    const std::string& GetToken() const { return token_; }

private:
    Dog* dog_;
    std::string token_;
};

class GameSession {
public:
    explicit GameSession(Map* map) : map_(map) {}

    Map* GetMap() { return map_; }
    const Map* GetMap() const { return map_; }

    Player* AddPlayer(const std::string& name, const std::string& token) {
        const Dog* dog = map_->AddDog(name);
        players_.emplace_back(const_cast<Dog*>(dog), token);
        return &players_.back();
    }

    const std::vector<Player>& GetPlayers() const { return players_; }

    Player* FindPlayerByToken(const std::string& token) {
        for (auto& player : players_) {
            if (player.GetToken() == token) {
                return &player;
            }
        }
        return nullptr;
    }

void Tick(int time_delta_ms) {
        double dt = time_delta_ms / 1000.0;
        const double half_width = 0.4;
        for (auto& dog : map_->GetDogs()) {
            auto [sx, sy] = dog.GetSpeed();
            if (sx == 0.0 && sy == 0.0) continue;
            auto [px, py] = dog.GetPosition();
            double new_x = px + sx * dt;
            double new_y = py + sy * dt;
            bool on_road = false;
            for (const auto& road : map_->GetRoads()) {
                auto start = road.GetStart(); auto end = road.GetEnd();
                if (road.IsHorizontal()) {
                    double min_x = std::min<double>(start.x, end.x);
                    double max_x = std::max<double>(start.x, end.x);
                    double ry = static_cast<double>(start.y);
                    if (new_y >= ry - half_width && new_y <= ry + half_width) {
                        if (new_x >= min_x - half_width && new_x <= max_x + half_width) {
                            on_road = true;
                            new_x = std::clamp(new_x, min_x - half_width, max_x + half_width);
                            break;
                        }
                    }
                } else {
                    double min_y = std::min<double>(start.y, end.y);
                    double max_y = std::max<double>(start.y, end.y);
                    double rx = static_cast<double>(start.x);
                    if (new_x >= rx - half_width && new_x <= rx + half_width) {
                        if (new_y >= min_y - half_width && new_y <= max_y + half_width) {
                            on_road = true;
                            new_y = std::clamp(new_y, min_y - half_width, max_y + half_width);
                            break;
                        }
                    }
                }
            }
            if (on_road) { dog.SetPosition(new_x, new_y); }
            else { dog.SetSpeed(0.0, 0.0); }
        }
    }

private:
    Map* map_;
    std::vector<Player> players_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept { return maps_; }

    void SetDefaultDogSpeed(double speed) { default_dog_speed_ = speed; }
    double GetDefaultDogSpeed() const { return default_dog_speed_; }

    Map* FindMap(const Map::Id& id) noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    GameSession* FindSession(const Map::Id& map_id) {
        if (auto it = sessions_.find(map_id); it != sessions_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    GameSession& GetOrCreateSession(const Map::Id& map_id, Map* map) {
        auto [it, inserted] = sessions_.try_emplace(map_id, map);
        return it->second;
    }

    static std::string GenerateToken() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dist;
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 0; i < 2; ++i) {
            ss << std::setw(16) << dist(gen);
        }
        return ss.str();
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;
    std::unordered_map<Map::Id, GameSession, MapIdHasher> sessions_;
    double default_dog_speed_ = 1.0;
};

}
// namespace model

