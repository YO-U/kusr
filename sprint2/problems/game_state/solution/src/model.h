#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <optional>
#include <array>

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
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

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
    std::pair<double, double> speed_;
    std::string dir_;
};

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

    const Dog* AddDog(const std::string& name, bool randomize_spawn) {
        dogs_.emplace_back();
        Dog& dog = dogs_.back();
        dog.SetName(name);
        if (!roads_.empty()) {
            if (randomize_spawn) {
                static thread_local std::random_device rd;
                static thread_local std::mt19937 gen(rd());
                std::uniform_int_distribution<size_t> road_dist(0, roads_.size() - 1);
                const auto& road = roads_[road_dist(gen)];
                const auto start = road.GetStart();
                const auto end = road.GetEnd();
                std::uniform_real_distribution<double> t_dist(0.0, 1.0);
                const double t = t_dist(gen);
                dog.SetPosition(start.x + t * (end.x - start.x), start.y + t * (end.y - start.y));
            } else {
                const auto start = roads_[0].GetStart();
                dog.SetPosition(static_cast<double>(start.x), static_cast<double>(start.y));
            }
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

    Player* AddPlayer(const std::string& name, const std::string& token, bool randomize_spawn) {
        const Dog* dog = map_->AddDog(name, randomize_spawn);
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
        const double dt = time_delta_ms / 1000.0;
        constexpr double half_width = 0.4;

        auto road_rect = [half_width](const Road& road) {
            const auto start = road.GetStart();
            const auto end = road.GetEnd();
            const double min_x = std::min(start.x, end.x) - half_width;
            const double max_x = std::max(start.x, end.x) + half_width;
            const double min_y = std::min(start.y, end.y) - half_width;
            const double max_y = std::max(start.y, end.y) + half_width;
            return std::array<double, 4>{min_x, min_y, max_x, max_y};
        };

        auto on_road = [](const std::array<double, 4>& r, double x, double y) {
            return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3];
        };

        auto clamp_to_road = [](const std::array<double, 4>& r, double x, double y) {
            return std::pair{std::clamp(x, r[0], r[2]), std::clamp(y, r[1], r[3])};
        };

        auto dist2 = [](double x0, double y0, double x1, double y1) {
            const double dx = x1 - x0;
            const double dy = y1 - y0;
            return dx * dx + dy * dy;
        };

        for (auto& dog : map_->GetDogs()) {
            const auto [sx, sy] = dog.GetSpeed();
            if (sx == 0.0 && sy == 0.0) {
                continue;
            }
            const auto [px, py] = dog.GetPosition();
            const double estimated_x = px + sx * dt;
            const double estimated_y = py + sy * dt;

            std::optional<std::pair<double, double>> best;
            double best_d2 = -1.0;

            for (const auto& road : map_->GetRoads()) {
                const auto rect = road_rect(road);
                if (!on_road(rect, px, py)) {
                    continue;
                }
                const auto [cx, cy] = clamp_to_road(rect, estimated_x, estimated_y);
                const double d2 = dist2(px, py, cx, cy);
                if (!best || d2 > best_d2) {
                    best = std::pair{cx, cy};
                    best_d2 = d2;
                }
            }

            if (!best) {
                dog.SetSpeed(0.0, 0.0);
                continue;
            }

            dog.SetPosition(best->first, best->second);
            if (best->first != estimated_x || best->second != estimated_y) {
                dog.SetSpeed(0.0, 0.0);
            }
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
    Maps& GetMaps() noexcept { return maps_; }

    void SetDefaultDogSpeed(double speed) { default_dog_speed_ = speed; }
    double GetDefaultDogSpeed() const { return default_dog_speed_; }

    void SetRandomizeSpawnPoints(bool value) { randomize_spawn_ = value; }
    bool GetRandomizeSpawnPoints() const { return randomize_spawn_; }

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
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        static thread_local std::uniform_int_distribution<uint64_t> dist;
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
    bool randomize_spawn_ = false;
};

}  // namespace model
