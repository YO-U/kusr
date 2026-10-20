#include "use_cases_impl.h"

#include <stdexcept>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../postgres/postgres.h"

#include <pqxx/zview.hxx>

namespace app {

using pqxx::operator"" _zv;
using namespace domain;

namespace {

AuthorInfo ToAuthorInfo(const Author& author) {
    return {author.GetId().ToString(), author.GetName()};
}

BookInfo ToBookInfo(const BookWithAuthor& book) {
    return {book.id.ToString(), book.title, book.publication_year, book.author_name};
}

}  // namespace

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name cannot be empty");
    }
    authors_.Save({AuthorId::New(), name});
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() const {
    std::vector<AuthorInfo> result;
    for (const auto& author : authors_.GetAll()) {
        result.push_back(ToAuthorInfo(author));
    }
    return result;
}

void UseCasesImpl::AddBook(const AddBookParams& params) {
    if (params.title.empty()) {
        throw std::invalid_argument("Book title cannot be empty");
    }

    auto& author_repo = static_cast<postgres::AuthorRepositoryImpl&>(authors_);
    auto& book_repo = static_cast<postgres::BookRepositoryImpl&>(books_);

    AuthorId author_id;
    std::optional<Author> author_to_create;
    if (params.author_id) {
        author_id = AuthorId::FromString(*params.author_id);
    } else if (params.author_name) {
        if (auto author = authors_.GetByName(*params.author_name)) {
            author_id = author->GetId();
        } else if (params.create_author_if_missing) {
            author_to_create.emplace(AuthorId::New(), *params.author_name);
            author_id = author_to_create->GetId();
        } else {
            throw std::runtime_error("Author not found");
        }
    } else {
        throw std::runtime_error("Author not selected");
    }

    pqxx::work work{*connection_};
    if (author_to_create) {
        author_repo.Save(work, *author_to_create);
    }
    Book book{BookId::New(), params.title, params.publication_year, author_id};
    book_repo.Save(work, book);
    book_repo.SetTags(work, book.GetId(), params.tags);
    work.commit();
}

std::vector<BookInfo> UseCasesImpl::GetBooks() const {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetAllWithAuthor()) {
        result.push_back(ToBookInfo(book));
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetByAuthorId(AuthorId::FromString(author_id))) {
        result.push_back({"", book.GetTitle(), book.GetPublicationYear(), ""});
    }
    return result;
}

std::optional<AuthorInfo> UseCasesImpl::FindAuthorByName(const std::string& name) const {
    if (auto author = authors_.GetByName(name)) {
        return ToAuthorInfo(*author);
    }
    return std::nullopt;
}

void UseCasesImpl::DeleteAuthor(const std::string& author_id) {
    auto& author_repo = static_cast<postgres::AuthorRepositoryImpl&>(authors_);
    auto& book_repo = static_cast<postgres::BookRepositoryImpl&>(books_);
    const AuthorId id = AuthorId::FromString(author_id);

    pqxx::work work{*connection_};
    book_repo.DeleteTagsForAuthorBooks(work, id);
    work.exec_params("DELETE FROM books WHERE author_id=$1;"_zv, id.ToString());
    author_repo.Delete(work, id);
    work.commit();
}

void UseCasesImpl::EditAuthor(const std::string& author_id, const std::string& new_name) {
    if (new_name.empty()) {
        throw std::invalid_argument("Author name cannot be empty");
    }
    authors_.UpdateName(AuthorId::FromString(author_id), new_name);
}

std::vector<BookInfo> UseCasesImpl::FindBooksByTitle(const std::string& title) const {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetByTitle(title)) {
        result.push_back(ToBookInfo(book));
    }
    return result;
}

std::optional<BookDetails> UseCasesImpl::GetBookDetails(const std::string& book_id) const {
    if (auto details = books_.GetDetails(BookId::FromString(book_id))) {
        return BookDetails{details->id.ToString(), details->title, details->author_name, details->publication_year,
                           details->tags};
    }
    return std::nullopt;
}

void UseCasesImpl::DeleteBook(const std::string& book_id) {
    books_.Delete(BookId::FromString(book_id));
}

void UseCasesImpl::EditBook(const EditBookParams& params) {
    auto details = books_.GetDetails(BookId::FromString(params.book_id));
    if (!details) {
        throw std::runtime_error("Book not found");
    }

    auto& book_repo = static_cast<postgres::BookRepositoryImpl&>(books_);
    pqxx::work work{*connection_};

    const std::string title = params.new_title && !params.new_title->empty() ? *params.new_title : details->title;
    const int year = params.new_year ? *params.new_year : details->publication_year;

    work.exec_params("UPDATE books SET title=$2, publication_year=$3 WHERE id=$1;"_zv, params.book_id, title, year);

    if (params.new_tags) {
        book_repo.SetTags(work, BookId::FromString(params.book_id), *params.new_tags);
    }

    work.commit();
}

}  // namespace app
