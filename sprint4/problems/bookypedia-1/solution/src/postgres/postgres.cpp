#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAll() const {
    pqxx::read_transaction txn{connection_};
    auto result = txn.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    std::vector<domain::Author> authors;
    authors.reserve(result.size());
    for (const auto& row : result) {
        authors.emplace_back(domain::AuthorId::FromString(row[0].c_str()), row[1].c_str());
    }
    return authors;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, title, publication_year, author_id) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET title=$2, publication_year=$3, author_id=$4;
)"_zv,
        book.GetId().ToString(), book.GetTitle(), book.GetPublicationYear(), book.GetAuthorId().ToString());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAll() const {
    pqxx::read_transaction txn{connection_};
    auto result = txn.exec("SELECT id, title, publication_year, author_id FROM books ORDER BY title;"_zv);
    std::vector<domain::Book> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        books.emplace_back(domain::BookId::FromString(row[0].c_str()), row[1].c_str(), row[2].as<int>(),
                           domain::AuthorId::FromString(row[3].c_str()));
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetByAuthorId(const domain::AuthorId& author_id) const {
    pqxx::read_transaction txn{connection_};
    auto result = txn.exec_params(
        "SELECT id, title, publication_year, author_id FROM books WHERE author_id=$1 ORDER BY publication_year, title;"_zv,
        author_id.ToString());
    std::vector<domain::Book> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        books.emplace_back(domain::BookId::FromString(row[0].c_str()), row[1].c_str(), row[2].as<int>(),
                           domain::AuthorId::FromString(row[3].c_str()));
    }
    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
CREATE TABLE IF NOT EXISTS books (
    id UUID PRIMARY KEY,
    title VARCHAR(100) NOT NULL,
    publication_year INT,
    author_id UUID REFERENCES authors(id)
);
)"_zv);
    work.commit();
}

}  // namespace postgres
