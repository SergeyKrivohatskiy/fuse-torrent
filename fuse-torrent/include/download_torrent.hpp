#ifndef _DOWNLOAD_TORRENT_HPP
#define _DOWNLOAD_TORRENT_HPP
#include <filesystem>


void downloadTorrent(
		std::filesystem::path const &torrentFile,
		std::filesystem::path const &targetDirectory,
		std::filesystem::path const &mappingDirectory);

#endif // _DOWNLOAD_TORRENT_HPP
