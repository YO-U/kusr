#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockBookRepository : domain::BookRepository {
    void Save(const domain::Book&) override {
    }
    std::vector<domain::Book> GetAll() const override {
        return {};
    }
    std::vector<domain::Book> GetByAuthorId(const domain::AuthorId&) const override {
        return {};
    }
    std::vector<domain::BookWithAuthor> GetAllWithAuthor() const override {
        return {};
    }
    std::vector<domain::BookWithAuthor> GetByTitle(const std::string&) const override {
        return {};
    }
    std::optional<domain::BookDetails> GetDetails(const domain::BookId&) const override {
        return std::nullopt;
    }
    void Delete(const domain::BookId&) override {
    }
    void Update(const domain::BookId&, const std::string&, int) override {
    }
    void SetTags(const domain::BookId&, const std::vector<std::string>&) override {
    }
};

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }

    std::vector<domain::Author> GetAll() const override {
        return saved_authors;
    }

    std::optional<domain::Author> GetByName(const std::string&) const override {
        return std::nullopt;
    }

    void Delete(const domain::AuthorId&) override {
    }

    void UpdateName(const domain::AuthorId&, const std::string&) override {
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{nullptr, authors, books};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}
