#include "FuseTorrent.hpp"


namespace detail
{

FuseTorrent::FuseTorrent(std::filesystem::path const &torrentFile,
        std::filesystem::path const &targetDirectory):
    m_downloadProgress(indicators::option::BarWidth(50),
            indicators::option::Start("["), indicators::option::Fill("#"),
            indicators::option::Lead("#"), indicators::option::End("]"),
            indicators::option::ShowPercentage(true),
            indicators::option::PostfixText("download progress"),
            indicators::option::ShowElapsedTime(true),
            indicators::option::ShowRemainingTime(true)),
    m_pieceProgress(indicators::option::BarWidth(50),
            indicators::option::Start("["), indicators::option::Fill("#"),
            indicators::option::Lead("#"), indicators::option::End("]"),
            indicators::option::ShowPercentage(true),
            indicators::option::PostfixText("piece request")),
    m_progressBars(m_downloadProgress, m_pieceProgress),
    m_ltSession(),
    m_torrentInfo(torrentFile.generic_string()),
    m_pathResolver(m_torrentInfo.files()),
    m_torrentHandle(m_ltSession.add_torrent(m_torrentInfo,
            targetDirectory.generic_string(), lt::entry(),
            lt::storage_mode_t::storage_mode_allocate)),
    m_mutex(),
    m_pieceRequiests(),
    m_pieceCache(),
    m_torrentDownloadThread()
{
    m_pieceProgress.set_progress(100);
    for (lt::piece_index_t idx = 0; idx < m_torrentInfo.num_pieces(); ++idx) {
        m_torrentHandle.piece_priority(idx, lt::low_priority);
    }
    m_torrentDownloadThread = std::thread([this]() {
        torrentDownloadCycle();
    });
}


FuseTorrent::~FuseTorrent()
{
    if (m_torrentDownloadThread.joinable()) {
        m_torrentDownloadThread.join();
    }
}


int FuseTorrent::getattr(const char *path, fuse_stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct fuse_stat));

    if (m_pathResolver.hasDir(path)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    int const fIdx = m_pathResolver.fileIdx(path);
    if (fIdx != -1) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = m_torrentInfo.files().file_size(fIdx);
        return 0;
    }

    return -ENOENT;
}


int FuseTorrent::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        fuse_off_t off, fuse_file_info *fi)
{
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (m_pathResolver.hasDir(path)) {
        for (std::string const &subPath: m_pathResolver.dirContent(path)) {
            filler(buf, subPath.c_str(), NULL, 0);
        }
    }

    return 0;
}


int FuseTorrent::open(const char *path, fuse_file_info *fi)
{
    int const fIdx = m_pathResolver.fileIdx(path);
    if (fIdx != -1) {
        fi->fh = fIdx;
        return 0;
    }
    assert(false);
    return -ENOENT;
}


int FuseTorrent::read(const char *path, char *buf, size_t const sizeRequested,
        fuse_off_t const off, fuse_file_info *fi)
{
    int const fIdx = static_cast<int>(fi->fh);
    lt::file_storage const &fs = m_torrentInfo.files();
    if (fIdx >= fs.num_files()) {
        return -EBADF;
    }

    int64_t const fileSize = fs.file_size(fIdx);
    if (off == fileSize) {
        return 0;
    }
    assert(off < fileSize);
    int64_t const readSize =
            std::min(static_cast<int64_t>(sizeRequested), fileSize - off);

    lt::peer_request const peerRequest = fs.map_file(fIdx, off, readSize);

    PieceData const data = loadWithCache(peerRequest.piece);

    int const maxRead = std::min(peerRequest.length,
            m_torrentInfo.piece_size(peerRequest.piece) - peerRequest.start);
    memcpy(buf, data.get() + peerRequest.start, maxRead);
    return maxRead;
}


void FuseTorrent::torrentDownloadCycle()
{
    while (true) {
        std::vector<lt::alert *> alerts;
        m_ltSession.pop_alerts(&alerts);

        for (lt::alert const *a: alerts) {
            if (lt::read_piece_alert const *rpa =
                            lt::alert_cast<lt::read_piece_alert>(a)) {
                std::scoped_lock<std::mutex> lock(m_mutex);
                auto it = m_pieceRequiests.find(rpa->piece);
                if (it != m_pieceRequiests.end()) {
                    it->second.promise.set_value(rpa->buffer);
                    m_pieceRequiests.erase(it);
                }
            }
            if (lt::alert_cast<lt::torrent_finished_alert>(a)) {
                break;
            }
            if (lt::alert_cast<lt::torrent_error_alert>(a)) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        lt::torrent_status const status =
                m_torrentHandle.status(lt::status_flags_t());
        m_progressBars.set_progress<0>(
                static_cast<size_t>(status.progress * 100));
    }
}


PieceData FuseTorrent::loadWithCache(lt::piece_index_t const pIdx)
{
    PieceData const *value = m_pieceCache.get(pIdx);
    if (value) {
        return *value;
    }
    return m_pieceCache.insert(pIdx, loadFromTorrent(pIdx));
}


PieceData FuseTorrent::loadFromTorrent(lt::piece_index_t const pIdx)
{

    std::shared_future<PieceData> pieceDataFuture;
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        auto it = m_pieceRequiests.find(pIdx);
        if (it == m_pieceRequiests.end()) {
            it = m_pieceRequiests.emplace(pIdx, PieceRequest()).first;
            it->second.future = it->second.promise.get_future();
        }
        pieceDataFuture = it->second.future;
    }
    m_torrentHandle.piece_priority(pIdx, lt::top_priority);
    m_torrentHandle.set_piece_deadline(
            pIdx, 0, lt::torrent_handle::alert_when_available);
    for (lt::piece_index_t pOff = 1; lt::top_priority - pOff != 0 &&
            pIdx + pOff < m_torrentInfo.num_pieces();
            ++pOff) {
        m_torrentHandle.piece_priority(pIdx + pOff, lt::top_priority - pOff);
    }

    m_progressBars.set_progress<1>(size_t(0));
    while (!m_torrentHandle.have_piece(pIdx)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<lt::partial_piece_info> const downloadQueue =
                m_torrentHandle.get_download_queue();
        auto it = std::find_if(downloadQueue.begin(), downloadQueue.end(),
                [pIdx](lt::partial_piece_info const &pi) {
                    return pi.piece_index == pIdx;
                });
        if (it != downloadQueue.end()) {
            size_t const progress = it->finished * 100 / it->blocks_in_piece;
            m_progressBars.set_progress<1>(progress);
        }
    }
    m_progressBars.set_progress<1>(size_t(100));

    return pieceDataFuture.get();
}

}
// namespace detail
