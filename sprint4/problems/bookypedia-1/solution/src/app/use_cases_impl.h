#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<AuthorInfo> GetAuthors() const override;
    void AddBook(const std::string& title, int publication_year, const std::string& author_id) override;
    std::vector<BookInfo> GetBooks() const override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
