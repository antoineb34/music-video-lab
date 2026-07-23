#include "result.hpp"
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

namespace {

struct Point {
    int x = 0;
    int y = 0;
};

} // namespace

TEST_CASE("Result: holds a successful value", "[result]")
{
    mvlab::Result<int> r{42};
    REQUIRE(r.has_value());
    REQUIRE(static_cast<bool>(r));
    CHECK(r.value() == 42);
}

TEST_CASE("Result: holds a failed Error", "[result]")
{
    mvlab::Error err{mvlab::ErrorCode::invalid_argument, "bad", std::nullopt};
    mvlab::Result<int> r{err};
    REQUIRE_FALSE(r.has_value());
    REQUIRE_FALSE(static_cast<bool>(r));
    CHECK(r.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(r.error().message == "bad");
}

TEST_CASE("Result: const access works for both a successful and failed Result", "[result]")
{
    const mvlab::Result<Point> ok{Point{1, 2}};
    CHECK(ok.value().x == 1);
    CHECK(ok.value().y == 2);

    const mvlab::Result<Point> failed{mvlab::Error{mvlab::ErrorCode::internal_error, "oops", std::nullopt}};
    CHECK(failed.error().code == mvlab::ErrorCode::internal_error);
}

TEST_CASE("Result: supports move-only value types", "[result]")
{
    mvlab::Result<std::unique_ptr<int>> r{std::make_unique<int>(7)};
    REQUIRE(r.has_value());
    auto ptr = std::move(r).value();
    REQUIRE(ptr != nullptr);
    CHECK(*ptr == 7);
}

TEST_CASE("Result: move construction and move assignment", "[result]")
{
    mvlab::Result<std::string> a{std::string("hello")};
    mvlab::Result<std::string> b{std::move(a)};
    REQUIRE(b.has_value());
    CHECK(b.value() == "hello");

    mvlab::Result<std::string> c{mvlab::Error{mvlab::ErrorCode::invalid_argument, "x", std::nullopt}};
    REQUIRE_FALSE(c.has_value());
    c = std::move(b);
    REQUIRE(c.has_value());
    CHECK(c.value() == "hello");
}

TEST_CASE("Result: wrong-alternative access throws instead of returning a default value", "[result]")
{
    mvlab::Result<int> failed{mvlab::Error{mvlab::ErrorCode::invalid_argument, "bad", std::nullopt}};
    CHECK_THROWS_AS(failed.value(), std::bad_variant_access);

    mvlab::Result<int> ok{5};
    CHECK_THROWS_AS(ok.error(), std::bad_variant_access);
}

TEST_CASE("Result<void>: success has no error", "[result]")
{
    mvlab::Result<void> r;
    REQUIRE(r.has_value());
    REQUIRE(static_cast<bool>(r));
}

TEST_CASE("Result<void>: failure carries an Error", "[result]")
{
    mvlab::Result<void> r{mvlab::Error{mvlab::ErrorCode::filesystem_error, "disk full", std::nullopt}};
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code == mvlab::ErrorCode::filesystem_error);
}

TEST_CASE("Result<void>: accessing error() on success throws instead of a default Error", "[result]")
{
    mvlab::Result<void> r;
    CHECK_THROWS_AS(r.error(), std::bad_optional_access);
}
