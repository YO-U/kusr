#include "model.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace {

constexpr double ROAD_HALF_WIDTH = 0.4;
constexpr double DOG_HALF_WIDTH = 0.3;
constexpr double LOOT_HALF_WIDTH = 0.0;
constexpr double OFFICE_HALF_WIDTH = 0.25;

std::array<double, 4> RoadRect(const Road& road) {
    const auto start = road.GetStart();
    const auto end = road.GetEnd();
    const double min_x = std::min(start.x, end.x) - ROAD_HALF_WIDTH;
    const double max_x = std::max(start.x, end.x) + ROAD_HALF_WIDTH;
    const double min_y = std::min(start.y, end.y) - ROAD_HALF_WIDTH;
    const double max_y = std::max(start.y, end.y) + ROAD_HALF_WIDTH;
    return {min_x, min_y, max_x, max_y};
}

bool IsOnRoad(const std::array<double, 4>& road_rect, double x, double y) {
    return x >= road_rect[0] && x <= road_rect[2] && y >= road_rect[1] && y <= road_rect[3];
}

std::pair<double, double> ClampToRoad(const std::array<double, 4>& road_rect, double x, double y) {
    return {std::clamp(x, road_rect[0], road_rect[2]), std::clamp(y, road_rect[1], road_rect[3])};
}

double DistanceSquared(double x0, double y0, double x1, double y1) {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    return dx * dx + dy * dy;
}

class SessionGatheringProvider : public collision_detector::ItemGathererProvider {
public:
    SessionGatheringProvider(const std::vector<LostObject>& lost_objects,
                             const Map::Offices& offices,
                             const std::vector<std::pair<double, double>>& start_positions,
                             const std::deque<Player>& players)
        : lost_objects_(lost_objects)
        , offices_(offices)
        , start_positions_(start_positions)
        , players_(players) {
    }

    size_t ItemsCount() const override {
        return lost_objects_.size() + offices_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        if (idx < lost_objects_.size()) {
            const auto& object = lost_objects_[idx];
            return {{object.position.first, object.position.second}, LOOT_HALF_WIDTH};
        }
        const auto office_pos = offices_[idx - lost_objects_.size()].GetPosition();
        return {{static_cast<double>(office_pos.x), static_cast<double>(office_pos.y)}, OFFICE_HALF_WIDTH};
    }

    size_t GatherersCount() const override {
        return players_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        const auto& start = start_positions_[idx];
        const auto& end = players_[idx].GetDog()->GetPosition();
        return {{start.first, start.second}, {end.first, end.second}, DOG_HALF_WIDTH};
    }

private:
    const std::vector<LostObject>& lost_objects_;
    const Map::Offices& offices_;
    const std::vector<std::pair<double, double>>& start_positions_;
    const std::deque<Player>& players_;
};

}  // namespace

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }
    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

std::pair<double, double> Map::GetRandomRoadPoint() const {
    if (roads_.empty()) {
        return {0.0, 0.0};
    }

    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> road_dist(0, roads_.size() - 1);
    const auto& road = roads_[road_dist(gen)];
    const auto start = road.GetStart();
    const auto end = road.GetEnd();
    std::uniform_real_distribution<double> t_dist(0.0, 1.0);
    const double t = t_dist(gen);
    return {start.x + t * (end.x - start.x), start.y + t * (end.y - start.y)};
}

void GameSession::Tick(int time_delta_ms) {
    const auto time_delta = std::chrono::milliseconds(time_delta_ms);
    if (map_->GetLootTypeCount() > 0) {
        const unsigned generated = loot_generator_.Generate(
            time_delta,
            static_cast<unsigned>(lost_objects_.size()),
            static_cast<unsigned>(players_.size()));

        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> type_dist(0, map_->GetLootTypeCount() - 1);
        for (unsigned i = 0; i < generated; ++i) {
            lost_objects_.push_back(LostObject{
                .id = next_lost_object_id_++,
                .type = type_dist(gen),
                .position = map_->GetRandomRoadPoint(),
            });
        }
    }

    const double dt = time_delta_ms / 1000.0;
    std::vector<std::pair<double, double>> start_positions;
    start_positions.reserve(players_.size());
    for (const auto& player : players_) {
        start_positions.push_back(player.GetDog()->GetPosition());
    }

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
            const auto rect = RoadRect(road);
            if (!IsOnRoad(rect, px, py)) {
                continue;
            }
            const auto [cx, cy] = ClampToRoad(rect, estimated_x, estimated_y);
            const double d2 = DistanceSquared(px, py, cx, cy);
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

    std::vector<size_t> event_loot_ids;
    event_loot_ids.reserve(lost_objects_.size());
    for (const auto& object : lost_objects_) {
        event_loot_ids.push_back(object.id);
    }

    SessionGatheringProvider provider{lost_objects_, map_->GetOffices(), start_positions, players_};
    const auto events = collision_detector::FindGatherEvents(provider);
    const size_t loot_count = lost_objects_.size();

    for (const auto& event : events) {
        auto& player = players_[event.gatherer_id];
        if (event.item_id < loot_count) {
            if (!player.HasBagSpace(map_->GetBagCapacity())) {
                continue;
            }
            auto object_it = std::find_if(lost_objects_.begin(), lost_objects_.end(),
                                          [id = event_loot_ids[event.item_id]](const LostObject& object) {
                                              return object.id == id;
                                          });
            if (object_it == lost_objects_.end()) {
                continue;
            }
            player.AddToBag(BagItem{.id = object_it->id, .type = object_it->type});
            lost_objects_.erase(object_it);
        } else {
            player.ReturnBag(*map_);
        }
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

}  // namespace model