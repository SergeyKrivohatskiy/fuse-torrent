#ifndef FUSE_TORRENT_DETAIL_FUSE_TORRENT_HPP
#define FUSE_TORRENT_DETAIL_FUSE_TORRENT_HPP
#include "Cache.hpp"
#include "PathResolver.hpp"
#include "fuse.hpp"

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>

#include <boost/shared_array.hpp>

#include <indicators/multi_progress.hpp>
#include <indicators/progress_bar.hpp>

#include <cstddef>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>


namespace detail
{

using PieceData = boost::shared_array<char>;


struct PieceRequest
{
    std::promise<PieceData> promise;
    std::shared_future<PieceData> future;
};


class FuseTorrent
{
public:
    FuseTorrent(
            std::filesystem::path const &torrentFile,
            std::filesystem::path const &targetDirectory);
    ~FuseTorrent();

    FuseTorrent(FuseTorrent const &) = delete;
    FuseTorrent &operator=(FuseTorrent const &) = delete;

    int getattr(char const *path, fuse_stat *stbuf);
    int readdir(char const *path, void *buf, fuse_fill_dir_t filler,
            fuse_off_t off, fuse_file_info *fi);
    int open(char const *path, fuse_file_info *fi);
    int read(char const *path, char *buf, std::size_t size, fuse_off_t off,
            fuse_file_info *fi);

private:
    void torrentDownloadCycle(std::stop_token stopToken);
    void failPendingPieceRequests();

    PieceData loadWithCache(lt::piece_index_t pIdx);
    PieceData loadFromTorrent(lt::piece_index_t pIdx);
    std::shared_future<PieceData> placePieceRequest(lt::piece_index_t pIdx);
    PieceData waitForData(lt::piece_index_t pIdx,
            std::shared_future<PieceData> pieceDataFuture);

    int readFromPiece(char *buf, lt::peer_request const &peerRequest);

    void requestPieceDownload(lt::piece_index_t pIdx);

    void updateTorrentDownloadProgress();

private:
    static constexpr std::size_t PIECE_CACHE_CAPACITY = 32;

private:
    indicators::ProgressBar m_downloadProgress;
    indicators::ProgressBar m_pieceProgress;
    indicators::MultiProgress<indicators::ProgressBar, 2> m_progressBars;

    lt::session m_ltSession;
    lt::torrent_handle m_torrentHandle;
    std::shared_ptr<lt::torrent_info const> m_torrentInfo;

    PathResolver m_pathResolver;

    std::mutex m_progressMutex;

    std::mutex m_pieceRequestsMutex;
    std::map<lt::piece_index_t, PieceRequest> m_pieceRequests;

    std::mutex m_pieceCacheMutex;
    Cache<lt::piece_index_t, PieceData, PIECE_CACHE_CAPACITY> m_pieceCache;

    std::jthread m_torrentDownloadThread;
};

} // namespace detail

#endif // FUSE_TORRENT_DETAIL_FUSE_TORRENT_HPP
