#include "FuseTorrent.hpp"

#include <libtorrent/alert_types.hpp>


FuseTorrent::FuseTorrent(
        std::filesystem::path const& torrentFile,
        std::filesystem::path const& targetDirectory,
        std::filesystem::path const& mappingDirectory):
    m_downloadProgress(
            indicators::option::BarWidth(50),
            indicators::option::Start("["),
            indicators::option::Fill("#"),
            indicators::option::Lead("#"),
            indicators::option::End("]"),
            indicators::option::ShowPercentage(true)),
    m_progressBars(m_downloadProgress),
    m_ltSession(),
    m_torrentInfo(torrentFile.generic_string()),
    torrentHandle(m_ltSession.add_torrent(
        m_torrentInfo,
        targetDirectory.generic_string(),
        lt::entry(),
        lt::storage_mode_t::storage_mode_allocate)),
    m_torrentDownloadThread()
{
    m_progressBars.set_option(indicators::option::HideBarWhenComplete(true));
    for (lt::piece_index_t idx = 0; idx < m_torrentInfo.num_pieces(); ++idx) {
        torrentHandle.piece_priority(idx, lt::low_priority);
    }
}


FuseTorrent::~FuseTorrent()
{
    if (m_torrentDownloadThread.joinable()) {
        m_torrentDownloadThread.join();
    }
}


int FuseTorrent::start()
{
    m_torrentDownloadThread = std::thread([this]() { torrentDownloadCycle(); });
    m_torrentDownloadThread.join();
    return 0;
}


void FuseTorrent::torrentDownloadCycle()
{
    while (true) {
        std::vector<lt::alert*> alerts;
        m_ltSession.pop_alerts(&alerts);

        for (lt::alert const* a : alerts) {
            if (lt::read_piece_alert const* rpa =
                    lt::alert_cast<lt::read_piece_alert>(a)) {
                // TODO
            }
            if (lt::alert_cast<lt::torrent_finished_alert>(a)) {
                break;
            }
            if (lt::alert_cast<lt::torrent_error_alert>(a)) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        lt::torrent_status const status = torrentHandle.status(lt::status_flags_t());
        torrentDownloadProgress().set_progress(static_cast<size_t>(status.progress * 100));
    }
}


indicators::ProgressBar &FuseTorrent::torrentDownloadProgress()
{
    return m_progressBars[0];
}
