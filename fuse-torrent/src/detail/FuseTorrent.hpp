#ifndef _DETAIL_FUSE_TORRENT_HPP
#define _DETAIL_FUSE_TORRENT_HPP
#include "Cache.hpp"
#include "PathResolver.hpp"
#include "fuse.hpp"

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>

#include <indicators/progress_bar.hpp>
#include <indicators/multi_progress.hpp>

#include <thread>
#include <future>
#include <mutex>
#include <filesystem>


namespace detail
{

typedef boost::shared_array<char> PieceData;


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

    int getattr(const char *path, struct fuse_stat *stbuf);
    int readdir(
            const char *path, void *buf, fuse_fill_dir_t filler,
            fuse_off_t off, struct fuse_file_info *fi);
    int open(const char *path, struct fuse_file_info *);
    int read(const char *path, char *buf, size_t size, fuse_off_t off,
            struct fuse_file_info *fi);

private:
    void torrentDownloadCycle();
    
    PieceData loadWithCache(lt::piece_index_t pIdx);
    PieceData loadFromTorrent(lt::piece_index_t pIdx);
    std::shared_future<PieceData> placePieceRequest(lt::piece_index_t pIdx);
    PieceData waitForData(lt::piece_index_t pIdx,
            std::shared_future<PieceData> pieceDataFuture);

    void requestPieceDownload(lt::piece_index_t pIdx);

    void updateTorrentDownloadProgress();

private:
    indicators::ProgressBar m_downloadProgress;
    indicators::ProgressBar m_pieceProgress;
    indicators::MultiProgress<indicators::ProgressBar, 2> m_progressBars;

    lt::session m_ltSession;
    lt::torrent_info m_torrentInfo;
    lt::torrent_handle m_torrentHandle;

    detail::PathResolver m_pathResolver;

    std::mutex m_pieceRequiestsMutex;
    std::map<lt::piece_index_t, PieceRequest> m_pieceRequiests;

    detail::Cache<lt::piece_index_t, PieceData, 32> m_pieceCache;

    std::thread m_torrentDownloadThread;
};

}
// namespace detail

#endif // _DETAIL_FUSE_TORRENT_HPP
