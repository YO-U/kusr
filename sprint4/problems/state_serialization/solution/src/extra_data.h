#pragma once

#include "model.h"

#include <boost/json.hpp>

namespace extra_data {

void SetMapLootTypes(const model::Map::Id& map_id, boost::json::array loot_types);
boost::json::array GetMapLootTypes(const model::Map::Id& map_id);

}  // namespace extra_data
