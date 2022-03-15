#ifndef _FUSE_TORRENT_HPP
#define _FUSE_TORRENT_HPP
#include <filesystem>

int downloadTorrentWithFuseMapping(
        std::filesystem::path const& torrentFile,
        std::filesystem::path const& targetDirectory,
        std::filesystem::path const& mappingDirectory);

#endif // _FUSE_TORRENT_HPP
