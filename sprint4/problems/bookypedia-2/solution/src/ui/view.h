#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace menu {
class Menu;
}

namespace app {
class UseCases;
}

namespace ui {
namespace detail {

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

}  // namespace detail

class View {
public:
    View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output);

private:
    bool AddAuthor(std::istream& cmd_input) const;
    bool AddBook(std::istream& cmd_input) const;
    bool ShowAuthors() const;
    bool ShowBooks() const;
    bool ShowAuthorBooks() const;
    bool DeleteAuthor(std::istream& cmd_input) const;
    bool EditAuthor(std::istream& cmd_input) const;
    bool ShowBook(std::istream& cmd_input) const;
    bool DeleteBook(std::istream& cmd_input) const;
    bool EditBook(std::istream& cmd_input) const;

    std::optional<std::string> SelectAuthor() const;
    std::optional<std::string> SelectBook(const std::vector<detail::BookInfo>& books) const;
    std::optional<std::string> ResolveAuthor(std::istream& cmd_input) const;
    std::optional<std::string> ResolveBook(std::istream& cmd_input) const;

    std::vector<detail::AuthorInfo> GetAuthors() const;
    std::vector<detail::BookInfo> GetBooks() const;
    static std::vector<std::string> NormalizeTags(const std::string& tags);

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace ui
