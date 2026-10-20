#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/game_serialization.h"
#include "../src/model.h"

using namespace std::literals;

namespace {

model::Game MakeGameWithSession() {
    model::Game game;
    model::Map map{model::Map::Id{"map1"s}, "Map 1"s};
    map.AddRoad(model::Road{model::Road::HORIZONTAL, model::Point{0, 0}, 10});
    map.SetBagCapacity(3);
    map.SetLootValues({5, 10});
    game.AddMap(std::move(map));

    auto* stored_map = game.FindMap(model::Map::Id{"map1"s});
    auto& session = game.GetOrCreateSession(stored_map->GetId(), stored_map);
    auto* player = session.AddPlayer("Pluto"s, "0123456789abcdef0123456789abcdef"s, false);
    auto* dog = player->GetDog();
    dog->SetPosition(42.2, 12.5);
    dog->SetSpeed(2.3, -1.2);
    dog->SetDirection("R"s);
    player->SetScore(42);
    player->SetIdleTimeMs(100);
    player->SetPlayTimeMs(1000);
    player->SetBag({{.id = 10, .type = 2}});
    session.SetLostObjects({{.id = 7, .type = 1, .position = {1.5, 2.5}}}, 8);
    return game;
}

}  // namespace

SCENARIO("Game state JSON roundtrip") {
    GIVEN("a game with a player and lost objects") {
        model::Game original = MakeGameWithSession();

        WHEN("state is serialized and deserialized") {
            const auto json = game_serialization::SerializeGame(original);
            model::Game restored;
            {
                model::Map map{model::Map::Id{"map1"s}, "Map 1"s};
                map.AddRoad(model::Road{model::Road::HORIZONTAL, model::Point{0, 0}, 10});
                map.SetBagCapacity(3);
                map.SetLootValues({5, 10});
                restored.AddMap(std::move(map));
            }
            game_serialization::DeserializeGame(restored, json);

            THEN("players and lost objects match") {
                auto* session = restored.FindSession(model::Map::Id{"map1"s});
                REQUIRE(session != nullptr);
                REQUIRE(session->GetPlayers().size() == 1);

                const auto& player = session->GetPlayers().front();
                const auto* dog = player.GetDog();
                CHECK(player.GetToken() == "0123456789abcdef0123456789abcdef"s);
                CHECK(dog->GetName() == "Pluto"s);
                CHECK(dog->GetPosition().first == Catch::Approx(42.2));
                CHECK(dog->GetPosition().second == Catch::Approx(12.5));
                CHECK(dog->GetSpeed().first == Catch::Approx(2.3));
                CHECK(dog->GetSpeed().second == Catch::Approx(-1.2));
                CHECK(dog->GetDirection() == "R"s);
                CHECK(player.GetScore() == 42);
                CHECK(player.GetIdleTimeMs() == 100);
                CHECK(player.GetPlayTimeMs() == 1000);
                REQUIRE(player.GetBag().size() == 1);
                CHECK(player.GetBag().front().id == 10);
                CHECK(player.GetBag().front().type == 2);

                REQUIRE(session->GetLostObjects().size() == 1);
                CHECK(session->GetLostObjects().front().id == 7);
                CHECK(session->GetLostObjects().front().type == 1);
                CHECK(session->GetLostObjects().front().position.first == Catch::Approx(1.5));
                CHECK(session->GetLostObjects().front().position.second == Catch::Approx(2.5));
                CHECK(session->GetNextLostObjectId() == 8);
            }
        }
    }
}
