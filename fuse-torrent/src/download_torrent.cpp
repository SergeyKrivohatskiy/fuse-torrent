#include "download_torrent.hpp"

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>

// #include <indicators/progress_bar.hpp>

#include <iostream>
#include <thread>


void downloadTorrent(
		std::filesystem::path const &torrentFile,
		std::filesystem::path const &targetDirectory,
		std::filesystem::path const &mappingDirectory)
{
    libtorrent::session session;
    libtorrent::torrent_info torrentInfo(torrentFile.generic_string());

    libtorrent::torrent_handle torrentHandle = session.add_torrent(
            torrentInfo,
            targetDirectory.generic_string(),
            libtorrent::entry(),
            libtorrent::storage_mode_t::storage_mode_allocate);

    
    while (true) {
        std::vector<lt::alert*> alerts;
        session.pop_alerts(&alerts);

        for (lt::alert const* a : alerts) {
            std::cout << a->message() << std::endl;
            if (lt::alert_cast<lt::torrent_finished_alert>(a)) {
                break;
            }
            if (lt::alert_cast<lt::torrent_error_alert>(a)) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

 //   indicators::ProgressBar progressBar(
 //           indicators::option::BarWidth{ 50 },
 //           indicators::option::Start{ "[" },
 //           indicators::option::Fill{ "#" },
 //           indicators::option::Lead{ "#" },
 //           indicators::option::End{ "]" },
 //           indicators::option::PostfixText{ "Downloading" },
 //           indicators::option::ForegroundColor{ indicators::Color::green });
	//for (int i = 0; i < 100; ++i) {
 //       progressBar.set_progress(static_cast<float>(i));
 //       std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//}
}
