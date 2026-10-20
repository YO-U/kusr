#include <boost/json/src.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>

namespace {

using pqxx::operator"" _zv;

void EnsureBooksTable(pqxx::connection& connection) {
    pqxx::work work{connection};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id SERIAL PRIMARY KEY,
    title varchar(100) NOT NULL,
    author varchar(100) NOT NULL,
    year integer NOT NULL,
    ISBN char(13) UNIQUE
);
)"_zv);
    work.commit();
}

std::string TrimIsbn(std::string isbn) {
    while (!isbn.empty() && isbn.back() == ' ') {
        isbn.pop_back();
    }
    return isbn;
}

bool AddBook(pqxx::connection& connection, const boost::json::object& payload) {
    const auto title = std::string(payload.at("title").as_string());
    const auto author = std::string(payload.at("author").as_string());
    const int year = static_cast<int>(payload.at("year").as_int64());

    std::optional<std::string> isbn;
    const auto& isbn_value = payload.at("ISBN");
    if (!isbn_value.is_null()) {
        isbn = std::string(isbn_value.as_string());
    }

    try {
        pqxx::work work{connection};
        if (isbn) {
            work.exec_params(
                "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)",
                title, author, year, *isbn);
        } else {
            work.exec_params(
                "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, NULL)",
                title, author, year);
        }
        work.commit();
        return true;
    } catch (const pqxx::sql_error&) {
        return false;
    }
}

boost::json::array AllBooks(pqxx::connection& connection) {
    pqxx::read_transaction txn{connection};
    const auto rows = txn.exec(R"(
SELECT id, title, author, year, ISBN
FROM books
ORDER BY year DESC, title ASC, author ASC, ISBN ASC
)"_zv);

    boost::json::array books;

    for (const auto& row : rows) {
        boost::json::object book;
        book["id"] = row["id"].as<int>();
        book["title"] = row["title"].as<std::string>();
        book["author"] = row["author"].as<std::string>();
        book["year"] = row["year"].as<int>();

        if (row["isbn"].is_null()) {
            book["ISBN"] = nullptr;
        } else {
            book["ISBN"] = TrimIsbn(row["isbn"].as<std::string>());
        }

        books.push_back(std::move(book));
    }

    return books;
}

void PrintResult(bool success) {
    boost::json::object response;
    response["result"] = success;
    std::cout << boost::json::serialize(response) << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: book_manager <postgres_connection_url>" << std::endl;
        return 1;
    }

    try {
        pqxx::connection connection{argv[1]};
        EnsureBooksTable(connection);

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }

            const auto value = boost::json::parse(line);
            const auto& root = value.as_object();
            const auto action = std::string_view(root.at("action").as_string());
            const auto& payload = root.at("payload").as_object();

            if (action == "add_book") {
                PrintResult(AddBook(connection, payload));
            } else if (action == "all_books") {
                std::cout << boost::json::serialize(AllBooks(connection)) << std::endl;
            } else if (action == "exit") {
                break;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
