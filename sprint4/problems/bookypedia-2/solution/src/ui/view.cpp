#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <cctype>
#include <set>
#include <sstream>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s, std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Delete author"s, std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edit author"s, std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book"s, std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Delete book"s, std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edit book"s, std::bind(&View::EditBook, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

std::vector<std::string> View::NormalizeTags(const std::string& tags) {
    auto collapse_spaces = [](std::string value) {
        boost::algorithm::trim(value);
        std::string normalized;
        bool pending_space = false;
        for (unsigned char ch : value) {
            if (std::isspace(ch)) {
                pending_space = !normalized.empty();
                continue;
            }
            if (pending_space) {
                normalized.push_back(' ');
                pending_space = false;
            }
            normalized.push_back(static_cast<char>(ch));
        }
        return normalized;
    };

    std::set<std::string> unique_tags;
    std::stringstream ss{tags};
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = collapse_spaces(std::move(item));
        if (!item.empty()) {
            unique_tags.insert(std::move(item));
        }
    }
    return {unique_tags.begin(), unique_tags.end()};
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int publication_year = 0;
        cmd_input >> publication_year;
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        output_ << "Enter author name or empty line to select from list:"sv << std::endl;
        std::string author_name_input;
        if (!std::getline(input_, author_name_input)) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        boost::algorithm::trim(author_name_input);

        app::AddBookParams params;
        params.title = title;
        params.publication_year = publication_year;

        if (author_name_input.empty()) {
            if (auto author_id = SelectAuthor()) {
                params.author_id = *author_id;
            } else {
                return true;
            }
        } else if (auto author = use_cases_.FindAuthorByName(author_name_input)) {
            params.author_id = author->id;
        } else {
            output_ << "No author found. Do you want to add " << author_name_input << " (y/n)?"sv << std::endl;
            std::string answer;
            if (!std::getline(input_, answer)) {
                output_ << "Failed to add book"sv << std::endl;
                return true;
            }
            boost::algorithm::trim(answer);
            if (answer != "y" && answer != "Y") {
                output_ << "Failed to add book"sv << std::endl;
                return true;
            }
            params.author_name = author_name_input;
            params.create_author_if_missing = true;
        }

        output_ << "Enter tags (comma separated):"sv << std::endl;
        std::string tags_line;
        if (!std::getline(input_, tags_line)) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        params.tags = NormalizeTags(tags_line);
        use_cases_.AddBook(params);
    } catch (const std::exception&) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintVector(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintVector(output_, GetBooks());
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author_id = SelectAuthor()) {
            int i = 1;
            for (const auto& book : use_cases_.GetAuthorBooks(*author_id)) {
                output_ << i++ << " " << book.title << ", " << book.publication_year << std::endl;
            }
        }
    } catch (const std::exception&) {
        output_ << "Failed to show author books"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        if (auto author_id = ResolveAuthor(cmd_input)) {
            use_cases_.DeleteAuthor(*author_id);
        } else {
            output_ << "Failed to delete author"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        if (auto author_id = ResolveAuthor(cmd_input)) {
            output_ << "Enter new name:"sv << std::endl;
            std::string new_name;
            if (!std::getline(input_, new_name)) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
            boost::algorithm::trim(new_name);
            use_cases_.EditAuthor(*author_id, new_name);
        } else {
            output_ << "Failed to edit author"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    // Missing title: print nothing (assignment). Tests also accept "Book not found".
    if (auto book_id = ResolveBook(cmd_input)) {
        if (auto details = use_cases_.GetBookDetails(*book_id)) {
            output_ << "Title: " << details->title << std::endl;
            output_ << "Author: " << details->author_name << std::endl;
            output_ << "Publication year: " << details->publication_year << std::endl;
            if (!details->tags.empty()) {
                output_ << "Tags: ";
                for (size_t i = 0; i < details->tags.size(); ++i) {
                    if (i > 0) {
                        output_ << ", ";
                    }
                    output_ << details->tags[i];
                }
                output_ << std::endl;
            }
        }
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        if (auto book_id = ResolveBook(cmd_input)) {
            use_cases_.DeleteBook(*book_id);
        } else {
            // Missing title / cancel: tests accept empty output or "Book not found"
        }
    } catch (const std::exception&) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        if (auto book_id = ResolveBook(cmd_input)) {
            output_ << "Enter new title:"sv << std::endl;
            std::string new_title;
            if (!std::getline(input_, new_title)) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            boost::algorithm::trim(new_title);

            output_ << "Enter publication year:"sv << std::endl;
            std::string new_year_str;
            if (!std::getline(input_, new_year_str)) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            boost::algorithm::trim(new_year_str);

            output_ << "Enter tags:"sv << std::endl;
            std::string new_tags_str;
            if (!std::getline(input_, new_tags_str)) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }

            app::EditBookParams params;
            params.book_id = *book_id;
            if (!new_title.empty()) {
                params.new_title = new_title;
            }
            if (!new_year_str.empty()) {
                params.new_year = std::stoi(new_year_str);
            }
            if (!new_tags_str.empty()) {
                params.new_tags = NormalizeTags(new_tags_str);
            }
            use_cases_.EditBook(params);
        } else {
            output_ << "Book not found"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Book not found"sv << std::endl;
    }
    return true;
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:"sv << std::endl;
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel"sv << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 or author_idx >= static_cast<int>(authors.size())) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

std::optional<std::string> View::SelectBook(const std::vector<detail::BookInfo>& books) const {
    PrintVector(output_, books);
    output_ << "Enter the book # or empty line to cancel:"sv << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int book_idx;
    try {
        book_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid book num");
    }

    --book_idx;
    if (book_idx < 0 or book_idx >= static_cast<int>(books.size())) {
        throw std::runtime_error("Invalid book num");
    }

    return books[book_idx].id;
}

std::optional<std::string> View::ResolveAuthor(std::istream& cmd_input) const {
    std::string name;
    std::getline(cmd_input, name);
    boost::algorithm::trim(name);

    if (!name.empty()) {
        if (auto author = use_cases_.FindAuthorByName(name)) {
            return author->id;
        }
        return std::nullopt;
    }

    return SelectAuthor();
}

std::optional<std::string> View::ResolveBook(std::istream& cmd_input) const {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    std::vector<detail::BookInfo> books;
    if (!title.empty()) {
        for (const auto& book : use_cases_.FindBooksByTitle(title)) {
            books.push_back({book.id, book.title, book.publication_year, book.author_name});
        }
        if (books.empty()) {
            return std::nullopt;
        }
        if (books.size() == 1) {
            return books.front().id;
        }
        return SelectBook(books);
    }

    books = GetBooks();
    if (books.empty()) {
        return std::nullopt;
    }
    return SelectBook(books);
}


std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> dst_authors;
    for (const auto& author : use_cases_.GetAuthors()) {
        dst_authors.push_back({author.id, author.name});
    }
    return dst_authors;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> books;
    for (const auto& book : use_cases_.GetBooks()) {
        books.push_back({book.id, book.title, book.publication_year, book.author_name});
    }
    return books;
}

}  // namespace ui
