#include "detail/Cache.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>


namespace
{

using TestCache = detail::Cache<int, std::string, 3>;

}


TEST_CASE("An empty cache finds nothing", "[Cache]")
{
    TestCache cache;

    REQUIRE(cache.get(1) == nullptr);
}


TEST_CASE("An inserted value is found again", "[Cache]")
{
    TestCache cache;
    cache.insert(1, "one");

    std::string const *const found = cache.get(1);

    REQUIRE(found != nullptr);
    REQUIRE(*found == "one");
    REQUIRE(cache.get(2) == nullptr);
}


TEST_CASE("Insert returns a reference to the stored value", "[Cache]")
{
    TestCache cache;

    std::string &stored = cache.insert(1, "one");
    stored += "!";

    REQUIRE(*cache.get(1) == "one!");
}


TEST_CASE("The least recently used entry is evicted at capacity", "[Cache]")
{
    TestCache cache;
    cache.insert(1, "one");
    cache.insert(2, "two");
    cache.insert(3, "three");

    cache.insert(4, "four");

    REQUIRE(cache.get(1) == nullptr);
    REQUIRE(*cache.get(2) == "two");
    REQUIRE(*cache.get(3) == "three");
    REQUIRE(*cache.get(4) == "four");
}


TEST_CASE("A lookup makes an entry the most recently used", "[Cache]")
{
    TestCache cache;
    cache.insert(1, "one");
    cache.insert(2, "two");
    cache.insert(3, "three");

    cache.get(1);
    cache.insert(4, "four");

    REQUIRE(*cache.get(1) == "one");
    REQUIRE(cache.get(2) == nullptr);
    REQUIRE(*cache.get(3) == "three");
    REQUIRE(*cache.get(4) == "four");
}


TEST_CASE("Inserting a present key replaces it without evicting", "[Cache]")
{
    TestCache cache;
    cache.insert(1, "one");
    cache.insert(2, "two");
    cache.insert(3, "three");

    cache.insert(1, "uno");

    REQUIRE(*cache.get(1) == "uno");
    REQUIRE(*cache.get(2) == "two");
    REQUIRE(*cache.get(3) == "three");
}


TEST_CASE("A replaced key becomes the most recently used", "[Cache]")
{
    TestCache cache;
    cache.insert(1, "one");
    cache.insert(2, "two");
    cache.insert(3, "three");

    cache.insert(1, "uno");
    cache.insert(4, "four");

    REQUIRE(cache.get(2) == nullptr);
    REQUIRE(*cache.get(1) == "uno");
    REQUIRE(*cache.get(4) == "four");
}
