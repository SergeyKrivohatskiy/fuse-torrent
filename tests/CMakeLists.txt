find_package(Catch2 3 CONFIG REQUIRED)

add_executable(fuse_torrent_tests)

target_sources(fuse_torrent_tests PRIVATE
               CacheTests.cpp
               PathResolverTests.cpp
               ${CMAKE_SOURCE_DIR}/src/detail/PathResolver.cpp)

target_include_directories(fuse_torrent_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_features(fuse_torrent_tests PRIVATE cxx_std_20)

target_link_libraries(fuse_torrent_tests PRIVATE
                      Catch2::Catch2WithMain
                      LibtorrentRasterbar::torrent-rasterbar)

if(MSVC)
    target_compile_options(fuse_torrent_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(fuse_torrent_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(Catch)
catch_discover_tests(fuse_torrent_tests)
