#include "detail/PathResolver.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>


namespace
{

lt::file_storage storageWith(std::initializer_list<char const *> const paths)
{
    lt::file_storage fs;
    fs.set_piece_length(16 * 1024);
    for (char const *const path: paths) {
        fs.add_file(path, 1024);
    }
    return fs;
}

}


TEST_CASE("The root of a single file torrent lists that file", "[PathResolver]")
{
    lt::file_storage const fs = storageWith({"payload.bin"});
    detail::PathResolver const resolver(fs);

    REQUIRE(resolver.hasDir("/"));
    REQUIRE(resolver.dirContent("/") ==
            detail::PathResolver::Files{"payload.bin"});
    REQUIRE(resolver.fileIdx("/payload.bin") == lt::file_index_t{0});
}


TEST_CASE("Unknown paths resolve to nothing", "[PathResolver]")
{
    lt::file_storage const fs = storageWith({"payload.bin"});
    detail::PathResolver const resolver(fs);

    REQUIRE_FALSE(resolver.fileIdx("/missing.bin").has_value());
    REQUIRE_FALSE(resolver.hasDir("/missing"));
    REQUIRE_FALSE(resolver.hasDir("/payload.bin"));
    REQUIRE_FALSE(resolver.fileIdx("/").has_value());
}


TEST_CASE("Every directory on the way to a file exists", "[PathResolver]")
{
    lt::file_storage const fs = storageWith({"torrent/a/b/deep.bin"});
    detail::PathResolver const resolver(fs);

    REQUIRE(resolver.hasDir("/"));
    REQUIRE(resolver.hasDir("/torrent"));
    REQUIRE(resolver.hasDir("/torrent/a"));
    REQUIRE(resolver.hasDir("/torrent/a/b"));
    REQUIRE(resolver.fileIdx("/torrent/a/b/deep.bin") == lt::file_index_t{0});

    REQUIRE(resolver.dirContent("/") == detail::PathResolver::Files{"torrent"});
    REQUIRE(resolver.dirContent("/torrent/a/b") ==
            detail::PathResolver::Files{"deep.bin"});
}


TEST_CASE("A directory lists all of its entries", "[PathResolver]")
{
    lt::file_storage const fs = storageWith(
            {"torrent/first.bin", "torrent/second.bin", "torrent/sub/third.bin"});
    detail::PathResolver const resolver(fs);

    REQUIRE(resolver.dirContent("/torrent") ==
            detail::PathResolver::Files{"first.bin", "second.bin", "sub"});
    REQUIRE(resolver.dirContent("/torrent/sub") ==
            detail::PathResolver::Files{"third.bin"});
}


TEST_CASE("Files keep the index they have in the storage", "[PathResolver]")
{
    lt::file_storage const fs = storageWith(
            {"torrent/first.bin", "torrent/second.bin", "torrent/sub/third.bin"});
    detail::PathResolver const resolver(fs);

    REQUIRE(resolver.fileIdx("/torrent/first.bin") == lt::file_index_t{0});
    REQUIRE(resolver.fileIdx("/torrent/second.bin") == lt::file_index_t{1});
    REQUIRE(resolver.fileIdx("/torrent/sub/third.bin") == lt::file_index_t{2});
}
