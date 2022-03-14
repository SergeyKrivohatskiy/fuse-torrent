#ifndef _FUSE_TORRENT_HPP
#define _FUSE_TORRENT_HPP
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>

#include <indicators/progress_bar.hpp>
#include <indicators/dynamic_progress.hpp>

#include <filesystem>
#include <thread>


class FuseTorrent
{
public:
	FuseTorrent(
			std::filesystem::path const& torrentFile,
			std::filesystem::path const& targetDirectory,
			std::filesystem::path const& mappingDirectory);
	~FuseTorrent();

	int start();

private:
	void torrentDownloadCycle();

	indicators::ProgressBar &torrentDownloadProgress();

private:
	indicators::ProgressBar m_downloadProgress;
	indicators::DynamicProgress<indicators::ProgressBar> m_progressBars;
	lt::session m_ltSession;
	lt::torrent_info m_torrentInfo;
	lt::torrent_handle torrentHandle;
	std::thread m_torrentDownloadThread;
};


#endif // _FUSE_TORRENT_HPP
