#include "extra_data.h"

#include <string>
#include <unordered_map>

namespace extra_data {

namespace {

std::unordered_map<std::string, boost::json::array>& LootTypesByMap() {
    static std::unordered_map<std::string, boost::json::array> data;
    return data;
}

}  // namespace

void SetMapLootTypes(const model::Map::Id& map_id, boost::json::array loot_types) {
    LootTypesByMap()[*map_id] = std::move(loot_types);
}

boost::json::array GetMapLootTypes(const model::Map::Id& map_id) {
    if (auto it = LootTypesByMap().find(*map_id); it != LootTypesByMap().end()) {
        return it->second;
    }
    return {};
}

}  // namespace extra_data
