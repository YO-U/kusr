#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <random>
#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <optional>
#include <array>
#include <chrono>
#include <numeric>

#include "collision_detector.h"
#include "loot_generator.h"
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

struct BagItem {
    size_t id = 0;
    size_t type = 0;
};

struct LostObject {
    size_t id = 0;
    size_t type = 0;
    std::pair<double, double> position;
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
    // deque keeps pointers/references to elements valid across push_back
    const std::deque<Dog>& GetDogs() const { return dogs_; }
    std::deque<Dog>& GetDogs() { return dogs_; }

    void SetDogSpeed(double speed) { dog_speed_ = speed; }
    double GetDogSpeed() const { return dog_speed_; }

    void SetBagCapacity(size_t capacity) { bag_capacity_ = capacity; }
    size_t GetBagCapacity() const { return bag_capacity_; }

    void SetLootTypeCount(size_t count) { loot_type_count_ = count; }
    size_t GetLootTypeCount() const { return loot_type_count_; }

    void SetLootValues(std::vector<int> values) { loot_values_ = std::move(values); }
    int GetLootValue(size_t type) const {
        return type < loot_values_.size() ? loot_values_[type] : 0;
    }

    std::pair<double, double> GetRandomRoadPoint() const;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    std::deque<Dog> dogs_;
    double dog_speed_ = 1.0;
    size_t bag_capacity_ = 3;
    size_t loot_type_count_ = 0;
    std::vector<int> loot_values_;
};

class Player {
public:
    Player(Dog* dog, const std::string& token) : dog_(dog), token_(token) {}

    Dog* GetDog() { return dog_; }
    const Dog* GetDog() const { return dog_; }
    const std::string& GetToken() const { return token_; }
    const std::vector<BagItem>& GetBag() const { return bag_; }
    int GetScore() const { return score_; }

    bool HasBagSpace(size_t capacity) const { return bag_.size() < capacity; }
    void AddToBag(BagItem item) { bag_.push_back(item); }
    void ReturnBag(const Map& map) {
        score_ = std::accumulate(bag_.begin(), bag_.end(), score_,
                                 [&map](int sum, const BagItem& item) {
                                     return sum + map.GetLootValue(item.type);
                                 });
        bag_.clear();
    }

private:
    Dog* dog_;
    std::string token_;
    std::vector<BagItem> bag_;
    int score_ = 0;
};

class GameSession {
public:
    GameSession(Map* map, loot_gen::LootGenerator::TimeInterval loot_period, double loot_probability)
        : map_(map)
        , loot_generator_(loot_period, loot_probability) {
    }

    Map* GetMap() { return map_; }
    const Map* GetMap() const { return map_; }

    Player* AddPlayer(const std::string& name, const std::string& token, bool randomize_spawn) {
        const Dog* dog = map_->AddDog(name, randomize_spawn);
        players_.emplace_back(const_cast<Dog*>(dog), token);
        return &players_.back();
    }

    const std::deque<Player>& GetPlayers() const { return players_; }
    const std::vector<LostObject>& GetLostObjects() const { return lost_objects_; }

    Player* FindPlayerByToken(const std::string& token) {
        auto it = std::find_if(players_.begin(), players_.end(),
                               [&token](const Player& player) { return player.GetToken() == token; });
        return it != players_.end() ? &*it : nullptr;
    }

    void Tick(int time_delta_ms);

private:
    Map* map_;
    std::deque<Player> players_;
    std::vector<LostObject> lost_objects_;
    size_t next_lost_object_id_ = 0;
    loot_gen::LootGenerator loot_generator_;
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

    void SetDefaultBagCapacity(size_t capacity) { default_bag_capacity_ = capacity; }
    size_t GetDefaultBagCapacity() const { return default_bag_capacity_; }

    void SetLootGeneratorConfig(loot_gen::LootGenerator::TimeInterval period, double probability) {
        loot_period_ = period;
        loot_probability_ = probability;
    }

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
        auto [it, inserted] = sessions_.try_emplace(map_id, map, loot_period_, loot_probability_);
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
    size_t default_bag_capacity_ = 3;
    loot_gen::LootGenerator::TimeInterval loot_period_{std::chrono::seconds{1}};
    double loot_probability_ = 0.0;
};

}  // namespace model
