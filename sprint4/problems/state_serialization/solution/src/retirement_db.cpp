#include "retirement_db.h"

#include <pqxx/pqxx>

namespace db {

RetirementDatabase::RetirementDatabase(std::shared_ptr<ConnectionPool> pool)
    : pool_{std::move(pool)} {
}

void RetirementDatabase::EnsureSchema() {
    auto conn = pool_->GetConnection();
    pqxx::work work{*conn};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS retired_players (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    score INTEGER NOT NULL,
    play_time DOUBLE PRECISION NOT NULL
);
)");
    work.exec(R"(
CREATE INDEX IF NOT EXISTS retired_players_score_time_name_idx
ON retired_players (score DESC, play_time ASC, name ASC);
)");
    work.commit();
}

void RetirementDatabase::InsertRetired(const std::string& name, int score, double play_time) {
    auto conn = pool_->GetConnection();
    pqxx::work work{*conn};
    work.exec_params(
        "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3)",
        name, score, play_time);
    work.commit();
}

std::vector<RetiredPlayerRow> RetirementDatabase::FetchRecords(int start, int max_items) {
    auto conn = pool_->GetConnection();
    pqxx::work work{*conn};
    const auto rows = work.exec_params(
        "SELECT name, score, play_time FROM retired_players "
        "ORDER BY score DESC, play_time ASC, name ASC "
        "LIMIT $1 OFFSET $2",
        max_items, start);
    work.commit();

    std::vector<RetiredPlayerRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(RetiredPlayerRow{
            row[0].as<std::string>(),
            row[1].as<int>(),
            row[2].as<double>(),
        });
    }
    return result;
}

}  // namespace db
