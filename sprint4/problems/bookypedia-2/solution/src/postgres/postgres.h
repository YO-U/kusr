#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAll() const override;
    std::optional<domain::Author> GetByName(const std::string& name) const override;
    void Delete(const domain::AuthorId& id) override;
    void UpdateName(const domain::AuthorId& id, const std::string& new_name) override;

    void Save(pqxx::work& work, const domain::Author& author);
    void Delete(pqxx::work& work, const domain::AuthorId& id);

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Book& book) override;
    std::vector<domain::Book> GetAll() const override;
    std::vector<domain::Book> GetByAuthorId(const domain::AuthorId& author_id) const override;
    std::vector<domain::BookWithAuthor> GetAllWithAuthor() const override;
    std::vector<domain::BookWithAuthor> GetByTitle(const std::string& title) const override;
    std::optional<domain::BookDetails> GetDetails(const domain::BookId& id) const override;
    void Delete(const domain::BookId& id) override;
    void Update(const domain::BookId& id, const std::string& title, int publication_year) override;
    void SetTags(const domain::BookId& id, const std::vector<std::string>& tags) override;

    void Save(pqxx::work& work, const domain::Book& book);
    void Delete(pqxx::work& work, const domain::BookId& id);
    void SetTags(pqxx::work& work, const domain::BookId& id, const std::vector<std::string>& tags);
    void DeleteTagsForAuthorBooks(pqxx::work& work, const domain::AuthorId& author_id);

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    pqxx::connection& GetConnection() & {
        return connection_;
    }

    AuthorRepositoryImpl& GetAuthors() & {
        return authors_;
    }

    BookRepositoryImpl& GetBooks() & {
        return books_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres
