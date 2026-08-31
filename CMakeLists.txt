cmake_minimum_required(VERSION 3.21...3.31)

project(FuseTorrent
        VERSION 0.2.0
        DESCRIPTION "A command line torrent client with FUSE file mapping"
        HOMEPAGE_URL "https://github.com/SergeyKrivohatskiy/fuse-torrent"
        LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

option(FUSE_TORRENT_BUILD_TESTS "Build the unit tests" OFF)

find_package(LibtorrentRasterbar CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(indicators CONFIG REQUIRED)

if(WIN32)
    find_package(WinFsp REQUIRED)
    add_library(FuseTorrent::fuse ALIAS WinFsp::WinFsp)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(fuse REQUIRED IMPORTED_TARGET fuse)
    add_library(FuseTorrent::fuse ALIAS PkgConfig::fuse)
endif()

add_executable(FuseTorrent)

target_sources(FuseTorrent PRIVATE
               src/main.cpp
               src/fuse_torrent.cpp
               src/fuse_torrent.hpp
               src/detail/Cache.hpp
               src/detail/FuseTorrent.cpp
               src/detail/FuseTorrent.hpp
               src/detail/PathResolver.cpp
               src/detail/PathResolver.hpp
               src/detail/fuse.hpp)

target_include_directories(FuseTorrent PRIVATE src)

target_compile_features(FuseTorrent PRIVATE cxx_std_20)

target_link_libraries(FuseTorrent PRIVATE
                      LibtorrentRasterbar::torrent-rasterbar
                      CLI11::CLI11
                      indicators::indicators
                      FuseTorrent::fuse)

if(MSVC)
    target_compile_options(FuseTorrent PRIVATE /W4 /permissive-)
    target_compile_definitions(FuseTorrent PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(FuseTorrent PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS FuseTorrent RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(FUSE_TORRENT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
