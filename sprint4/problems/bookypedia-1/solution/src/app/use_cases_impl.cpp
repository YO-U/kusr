#include "use_cases_impl.h"

#include <stdexcept>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name cannot be empty");
    }
    authors_.Save({AuthorId::New(), name});
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() const {
    std::vector<AuthorInfo> result;
    for (const auto& author : authors_.GetAll()) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

void UseCasesImpl::AddBook(const std::string& title, int publication_year, const std::string& author_id) {
    if (title.empty()) {
        throw std::invalid_argument("Book title cannot be empty");
    }
    books_.Save({BookId::New(), title, publication_year, AuthorId::FromString(author_id)});
}

std::vector<BookInfo> UseCasesImpl::GetBooks() const {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetAll()) {
        result.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetByAuthorId(AuthorId::FromString(author_id))) {
        result.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return result;
}

}  // namespace app
