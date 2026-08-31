#ifndef FUSE_TORRENT_FUSE_TORRENT_HPP
#define FUSE_TORRENT_FUSE_TORRENT_HPP
#include <filesystem>

int downloadTorrentWithFuseMapping(
        std::filesystem::path const &torrentFile,
        std::filesystem::path const &targetDirectory,
        std::filesystem::path const &mappingDirectory);

#endif // FUSE_TORRENT_FUSE_TORRENT_HPP
