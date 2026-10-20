#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../util/tagged_uuid.h"
#include "author_fwd.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, std::string title, int publication_year, AuthorId author_id)
        : id_(std::move(id))
        , title_(std::move(title))
        , publication_year_(publication_year)
        , author_id_(std::move(author_id)) {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

private:
    BookId id_;
    std::string title_;
    int publication_year_;
    AuthorId author_id_;
};

struct BookWithAuthor {
    BookId id;
    std::string title;
    int publication_year;
    std::string author_name;
};

struct BookDetails {
    BookId id;
    std::string title;
    std::string author_name;
    int publication_year;
    std::vector<std::string> tags;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual std::vector<Book> GetAll() const = 0;
    virtual std::vector<Book> GetByAuthorId(const AuthorId& author_id) const = 0;
    virtual std::vector<BookWithAuthor> GetAllWithAuthor() const = 0;
    virtual std::vector<BookWithAuthor> GetByTitle(const std::string& title) const = 0;
    virtual std::optional<BookDetails> GetDetails(const BookId& id) const = 0;
    virtual void Delete(const BookId& id) = 0;
    virtual void Update(const BookId& id, const std::string& title, int publication_year) = 0;
    virtual void SetTags(const BookId& id, const std::vector<std::string>& tags) = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain
