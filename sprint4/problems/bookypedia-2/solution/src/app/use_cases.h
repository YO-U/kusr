#pragma once

#include <optional>
#include <string>
#include <vector>

namespace app {

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string id;
    std::string title;
    int publication_year;
    std::string author_name;
};

struct BookDetails {
    std::string id;
    std::string title;
    std::string author_name;
    int publication_year;
    std::vector<std::string> tags;
};

struct AddBookParams {
    std::string title;
    int publication_year;
    std::optional<std::string> author_id;
    std::optional<std::string> author_name;
    bool create_author_if_missing = false;
    std::vector<std::string> tags;
};

struct EditBookParams {
    std::string book_id;
    std::optional<std::string> new_title;
    std::optional<int> new_year;
    std::optional<std::vector<std::string>> new_tags;
};

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<AuthorInfo> GetAuthors() const = 0;
    virtual void AddBook(const AddBookParams& params) = 0;
    virtual std::vector<BookInfo> GetBooks() const = 0;
    virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;
    virtual std::optional<AuthorInfo> FindAuthorByName(const std::string& name) const = 0;
    virtual void DeleteAuthor(const std::string& author_id) = 0;
    virtual void EditAuthor(const std::string& author_id, const std::string& new_name) = 0;
    virtual std::vector<BookInfo> FindBooksByTitle(const std::string& title) const = 0;
    virtual std::optional<BookDetails> GetBookDetails(const std::string& book_id) const = 0;
    virtual void DeleteBook(const std::string& book_id) = 0;
    virtual void EditBook(const EditBookParams& params) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
