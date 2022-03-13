#include "download_torrent.hpp"

#include <indicators/progress_bar.hpp>

#include <thread>


void downloadTorrent(
		std::filesystem::path const &torrentFile,
		std::filesystem::path const &targetDirectory,
		std::filesystem::path const &mappingDirectory)
{
	// TODO

    indicators::ProgressBar progressBar(
            indicators::option::BarWidth{ 50 },
            indicators::option::Start{ "[" },
            indicators::option::Fill{ "#" },
            indicators::option::Lead{ "#" },
            indicators::option::End{ "]" },
            indicators::option::PostfixText{ "Downloading" },
            indicators::option::ForegroundColor{ indicators::Color::green });
	for (int i = 0; i < 100; ++i) {
        progressBar.set_progress(static_cast<float>(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
