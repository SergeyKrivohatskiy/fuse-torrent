#include "FuseTorrent.hpp"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/load_torrent.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>


namespace detail
{

namespace
{

lt::add_torrent_params loadAddTorrentParams(
        std::filesystem::path const &torrentFile,
        std::filesystem::path const &targetDirectory)
{
    lt::add_torrent_params params =
            lt::load_torrent_file(torrentFile.generic_string());
    params.save_path = targetDirectory.generic_string();
    params.storage_mode = lt::storage_mode_allocate;
    return params;
}

} // namespace


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
            indicators::option::PostfixText("piece request processing")),
    m_progressBars(m_downloadProgress, m_pieceProgress),
    m_torrentHandle(m_ltSession.add_torrent(
            loadAddTorrentParams(torrentFile, targetDirectory))),
    m_torrentInfo(m_torrentHandle.torrent_file()),
    m_pathResolver(m_torrentInfo->layout())
{
    m_pieceProgress.set_progress(100);
    for (lt::piece_index_t const idx: m_torrentInfo->layout().piece_range()) {
        m_torrentHandle.piece_priority(idx, lt::low_priority);
    }
    m_torrentDownloadThread = std::jthread([this](std::stop_token stopToken) {
        torrentDownloadCycle(std::move(stopToken));
    });
}


FuseTorrent::~FuseTorrent()
{
    m_torrentDownloadThread.request_stop();
    failPendingPieceRequests();
}


int FuseTorrent::getattr(const char *path, fuse_stat *stbuf)
{
    std::memset(stbuf, 0, sizeof(fuse_stat));

    if (m_pathResolver.hasDir(path)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    std::optional<lt::file_index_t> const fIdx = m_pathResolver.fileIdx(path);
    if (fIdx) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = m_torrentInfo->layout().file_size(*fIdx);
        return 0;
    }

    return -ENOENT;
}


int FuseTorrent::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        fuse_off_t, fuse_file_info *)
{
    if (!m_pathResolver.hasDir(path)) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    for (std::string const &subPath: m_pathResolver.dirContent(path)) {
        filler(buf, subPath.c_str(), NULL, 0);
    }

    return 0;
}


int FuseTorrent::open(const char *path, fuse_file_info *fi)
{
    std::optional<lt::file_index_t> const fIdx = m_pathResolver.fileIdx(path);
    if (!fIdx) {
        return -ENOENT;
    }
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EACCES;
    }
    fi->fh = static_cast<std::uint64_t>(static_cast<int>(*fIdx));
    return 0;
}


int FuseTorrent::read(const char *, char *buf, std::size_t const sizeRequested,
        fuse_off_t const off, fuse_file_info *fi)
{
    lt::file_index_t const fIdx(static_cast<int>(fi->fh));
    lt::file_storage const &fs = m_torrentInfo->layout();
    if (static_cast<int>(fIdx) >= fs.num_files()) {
        return -EBADF;
    }
    if (off < 0) {
        return -EINVAL;
    }

    int64_t const fileSize = fs.file_size(fIdx);
    if (off >= fileSize) {
        return 0;
    }
    int64_t const readSize =
            std::min(static_cast<int64_t>(sizeRequested), fileSize - off);

    int64_t bytesRead = 0;
    while (bytesRead < readSize) {
        lt::peer_request const peerRequest = fs.map_file(fIdx, off + bytesRead,
                static_cast<int>(readSize - bytesRead));
        int const pieceBytes = readFromPiece(buf + bytesRead, peerRequest);
        if (pieceBytes < 0) {
            return bytesRead > 0 ? static_cast<int>(bytesRead) : pieceBytes;
        }
        bytesRead += pieceBytes;
    }

    return static_cast<int>(bytesRead);
}


int FuseTorrent::readFromPiece(char *buf, lt::peer_request const &peerRequest)
{
    PieceData const data = loadWithCache(peerRequest.piece);
    if (!data) {
        return -EIO;
    }

    int const availableInPiece =
            m_torrentInfo->piece_size(peerRequest.piece) - peerRequest.start;
    int const bytes = std::min(peerRequest.length, availableInPiece);
    std::memcpy(buf, data.get() + peerRequest.start, static_cast<std::size_t>(bytes));
    return bytes;
}


void FuseTorrent::torrentDownloadCycle(std::stop_token stopToken)
{
    std::vector<lt::alert *> alerts;
    while (!stopToken.stop_requested()) {
        m_ltSession.pop_alerts(&alerts);

        for (lt::alert const *a: alerts) {
            if (lt::read_piece_alert const *rpa =
                            lt::alert_cast<lt::read_piece_alert>(a)) {
                PieceData const data = rpa->error ? PieceData() : rpa->buffer;
                std::scoped_lock<std::mutex> lock(m_pieceRequestsMutex);
                auto it = m_pieceRequests.find(rpa->piece);
                if (it != m_pieceRequests.end()) {
                    it->second.promise.set_value(data);
                    m_pieceRequests.erase(it);
                }
            }
            if (lt::alert_cast<lt::torrent_error_alert>(a)) {
                failPendingPieceRequests();
            }
        }

        m_ltSession.wait_for_alert(std::chrono::milliseconds(200));

        updateTorrentDownloadProgress();
    }
}


void FuseTorrent::failPendingPieceRequests()
{
    std::scoped_lock<std::mutex> lock(m_pieceRequestsMutex);
    for (auto &pieceRequest: m_pieceRequests) {
        pieceRequest.second.promise.set_value(PieceData());
    }
    m_pieceRequests.clear();
}


PieceData FuseTorrent::loadWithCache(lt::piece_index_t const pIdx)
{
    {
        std::scoped_lock<std::mutex> lock(m_pieceCacheMutex);
        if (PieceData const *const cached = m_pieceCache.get(pIdx)) {
            return *cached;
        }
    }

    PieceData data = loadFromTorrent(pIdx);
    if (!data) {
        return data;
    }

    std::scoped_lock<std::mutex> lock(m_pieceCacheMutex);
    return m_pieceCache.insert(pIdx, std::move(data));
}


PieceData FuseTorrent::loadFromTorrent(lt::piece_index_t const pIdx)
{
    std::shared_future<PieceData> pieceDataFuture = placePieceRequest(pIdx);
    requestPieceDownload(pIdx);
    return waitForData(pIdx, std::move(pieceDataFuture));
}


std::shared_future<PieceData> FuseTorrent::placePieceRequest(
        lt::piece_index_t const pIdx)
{
    std::scoped_lock<std::mutex> lock(m_pieceRequestsMutex);
    auto it = m_pieceRequests.find(pIdx);
    if (it == m_pieceRequests.end()) {
        it = m_pieceRequests.emplace(pIdx, PieceRequest()).first;
        it->second.future = it->second.promise.get_future();
    }
    return it->second.future;
}


PieceData FuseTorrent::waitForData(
        lt::piece_index_t const pIdx,
        std::shared_future<PieceData> pieceDataFuture)
{
    {
        std::scoped_lock<std::mutex> lock(m_progressMutex);
        m_pieceProgress.set_option(indicators::option::Completed(false));
        m_progressBars.set_progress<1>(std::size_t{0});
    }
    while (pieceDataFuture.wait_for(std::chrono::milliseconds(60)) !=
            std::future_status::ready) {
        std::vector<lt::partial_piece_info> const downloadQueue =
                m_torrentHandle.get_download_queue();
        auto const it = std::ranges::find_if(downloadQueue,
                [pIdx](lt::partial_piece_info const &pieceInfo) {
                    return pieceInfo.piece_index == pIdx;
                });
        if (it != downloadQueue.end()) {
            std::size_t const progress = it->finished * 100 / it->blocks_in_piece;
            std::scoped_lock<std::mutex> lock(m_progressMutex);
            m_progressBars.set_progress<1>(progress);
        }
    }
    {
        std::scoped_lock<std::mutex> lock(m_progressMutex);
        m_progressBars.set_progress<1>(std::size_t{100});
    }

    return pieceDataFuture.get();
}


void FuseTorrent::requestPieceDownload(lt::piece_index_t pIdx)
{
    m_torrentHandle.piece_priority(pIdx, lt::top_priority);
    m_torrentHandle.set_piece_deadline(
            pIdx, 0, lt::torrent_handle::alert_when_available);
    int const topPriority = static_cast<std::uint8_t>(lt::top_priority);
    lt::piece_index_t const endPiece = m_torrentInfo->layout().end_piece();
    for (int pOff = 1; pOff < topPriority; ++pOff) {
        lt::piece_index_t const readAheadPiece =
                pIdx + lt::piece_index_t::diff_type(pOff);
        if (readAheadPiece >= endPiece) {
            break;
        }
        m_torrentHandle.piece_priority(readAheadPiece,
                lt::download_priority_t(
                        static_cast<std::uint8_t>(topPriority - pOff)));
    }
}


void FuseTorrent::updateTorrentDownloadProgress()
{
    lt::torrent_status const status =
            m_torrentHandle.status(lt::status_flags_t());
    std::scoped_lock<std::mutex> lock(m_progressMutex);
    m_progressBars.set_progress<0>(
            static_cast<std::size_t>(status.progress * 100));
}

} // namespace detail
