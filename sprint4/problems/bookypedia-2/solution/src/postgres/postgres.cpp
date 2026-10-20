#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>
#include <set>
#include <stdexcept>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    Save(work, author);
    work.commit();
}

void AuthorRepositoryImpl::Save(pqxx::work& work, const domain::Author& author) {
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
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

std::optional<domain::Author> AuthorRepositoryImpl::GetByName(const std::string& name) const {
    pqxx::read_transaction txn{connection_};
    auto result = txn.exec_params("SELECT id, name FROM authors WHERE name=$1;"_zv, name);
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result.front();
    return domain::Author{domain::AuthorId::FromString(row[0].c_str()), row[1].c_str()};
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    pqxx::work work{connection_};
    Delete(work, id);
    work.commit();
}

void AuthorRepositoryImpl::Delete(pqxx::work& work, const domain::AuthorId& id) {
    auto result = work.exec_params("DELETE FROM authors WHERE id=$1;"_zv, id.ToString());
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Failed to delete author");
    }
}

void AuthorRepositoryImpl::UpdateName(const domain::AuthorId& id, const std::string& new_name) {
    pqxx::work work{connection_};
    auto result = work.exec_params("UPDATE authors SET name=$2 WHERE id=$1;"_zv, id.ToString(), new_name);
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Failed to edit author");
    }
    work.commit();
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    Save(work, book);
    work.commit();
}

void BookRepositoryImpl::Save(pqxx::work& work, const domain::Book& book) {
    work.exec_params(
        R"(
INSERT INTO books (id, title, publication_year, author_id) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET title=$2, publication_year=$3, author_id=$4;
)"_zv,
        book.GetId().ToString(), book.GetTitle(), book.GetPublicationYear(), book.GetAuthorId().ToString());
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

std::vector<domain::BookWithAuthor> BookRepositoryImpl::GetAllWithAuthor() const {
    pqxx::read_transaction txn{connection_};
    auto result = txn.exec(
        R"(
SELECT books.id, title, publication_year, authors.name
FROM books
INNER JOIN authors ON authors.id = books.author_id
ORDER BY title, authors.name, publication_year;
)"_zv);
    std::vector<domain::BookWithAuthor> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        books.push_back({domain::BookId::FromString(row[0].c_str()), row[1].c_str(), row[2].as<int>(), row[3].c_str()});
    }
    return books;
}

std::vector<domain::BookWithAuthor> BookRepositoryImpl::GetByTitle(const std::string& title) const {
    pqxx::read_transaction txn{connection_};
    auto result = txn.exec_params(
        R"(
SELECT books.id, title, publication_year, authors.name
FROM books
INNER JOIN authors ON authors.id = books.author_id
WHERE title=$1
ORDER BY title, authors.name, publication_year;
)"_zv,
        title);
    std::vector<domain::BookWithAuthor> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        books.push_back({domain::BookId::FromString(row[0].c_str()), row[1].c_str(), row[2].as<int>(), row[3].c_str()});
    }
    return books;
}

std::optional<domain::BookDetails> BookRepositoryImpl::GetDetails(const domain::BookId& id) const {
    pqxx::read_transaction txn{connection_};
    auto result = txn.exec_params(
        R"(
SELECT books.id, title, authors.name, publication_year
FROM books
INNER JOIN authors ON authors.id = books.author_id
WHERE books.id=$1;
)"_zv,
        id.ToString());
    if (result.empty()) {
        return std::nullopt;
    }
    domain::BookDetails details{
        domain::BookId::FromString(result[0][0].c_str()),
        result[0][1].c_str(),
        result[0][2].c_str(),
        result[0][3].as<int>(),
        {},
    };
    auto tags_result = txn.exec_params("SELECT tag FROM book_tags WHERE book_id=$1 ORDER BY tag;"_zv, id.ToString());
    for (const auto& row : tags_result) {
        details.tags.push_back(row[0].c_str());
    }
    return details;
}

void BookRepositoryImpl::Delete(const domain::BookId& id) {
    pqxx::work work{connection_};
    Delete(work, id);
    work.commit();
}

void BookRepositoryImpl::Delete(pqxx::work& work, const domain::BookId& id) {
    work.exec_params("DELETE FROM book_tags WHERE book_id=$1;"_zv, id.ToString());
    work.exec_params("DELETE FROM books WHERE id=$1;"_zv, id.ToString());
}

void BookRepositoryImpl::Update(const domain::BookId& id, const std::string& title, int publication_year) {
    pqxx::work work{connection_};
    work.exec_params("UPDATE books SET title=$2, publication_year=$3 WHERE id=$1;"_zv, id.ToString(), title,
                     publication_year);
    work.commit();
}

void BookRepositoryImpl::SetTags(const domain::BookId& id, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    SetTags(work, id, tags);
    work.commit();
}

void BookRepositoryImpl::SetTags(pqxx::work& work, const domain::BookId& id, const std::vector<std::string>& tags) {
    work.exec_params("DELETE FROM book_tags WHERE book_id=$1;"_zv, id.ToString());
    for (const auto& tag : tags) {
        work.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);"_zv, id.ToString(), tag);
    }
}

void BookRepositoryImpl::DeleteTagsForAuthorBooks(pqxx::work& work, const domain::AuthorId& author_id) {
    work.exec_params(
        R"(
DELETE FROM book_tags
WHERE book_id IN (SELECT id FROM books WHERE author_id=$1);
)"_zv,
        author_id.ToString());
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
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID REFERENCES books(id),
    tag varchar(30) NOT NULL
);
)"_zv);
    work.commit();
}

}  // namespace postgres
