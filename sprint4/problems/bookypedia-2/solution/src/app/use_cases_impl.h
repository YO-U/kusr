#pragma once
#include <pqxx/connection>

#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    UseCasesImpl(pqxx::connection* connection, domain::AuthorRepository& authors, domain::BookRepository& books)
        : connection_{connection}
        , authors_{authors}
        , books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<AuthorInfo> GetAuthors() const override;
    void AddBook(const AddBookParams& params) override;
    std::vector<BookInfo> GetBooks() const override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const override;
    std::optional<AuthorInfo> FindAuthorByName(const std::string& name) const override;
    void DeleteAuthor(const std::string& author_id) override;
    void EditAuthor(const std::string& author_id, const std::string& new_name) override;
    std::vector<BookInfo> FindBooksByTitle(const std::string& title) const override;
    std::optional<BookDetails> GetBookDetails(const std::string& book_id) const override;
    void DeleteBook(const std::string& book_id) override;
    void EditBook(const EditBookParams& params) override;

private:
    pqxx::connection* connection_;
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
