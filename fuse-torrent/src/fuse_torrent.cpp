#include "fuse_torrent.hpp"

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>

#include <indicators/progress_bar.hpp>
#include <indicators/dynamic_progress.hpp>

#include <fuse/fuse.h>

#include <thread>
#include <future>
#include <mutex>
#include <deque>


namespace
{
    
typedef std::map<std::string, std::set<std::string>> DirsMap;
typedef std::map<std::string, int> FileToIdxMap;
struct PathsInfo
{
    DirsMap dirs;
    FileToIdxMap files;
};


typedef boost::shared_array<char> PieceData;


struct PieceRequest
{
    std::promise<PieceData> promise;
    std::shared_future<PieceData> future;
};


size_t MAX_CACHE_SIZE = 16;


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
    
    indicators::ProgressBar &torrentDownloadProgress();
    indicators::ProgressBar &pieceProgress();

private:
    std::filesystem::path m_mappingDirectory;
    indicators::ProgressBar m_downloadProgress;
    indicators::ProgressBar m_pieceProgress;
    indicators::DynamicProgress<indicators::ProgressBar> m_progressBars;
    lt::session m_ltSession;
    lt::torrent_info m_torrentInfo;
    PathsInfo m_pathMap;
    lt::torrent_handle m_torrentHandle;
    std::mutex m_mutex;
    std::map<lt::piece_index_t, PieceRequest> m_pieceRequiests;
    std::deque<std::pair<lt::piece_index_t, PieceData>> m_pieceCache;
    std::thread m_torrentDownloadThread;
};


std::vector<std::string> splitPath(std::string const &path)
{
    std::vector<std::string> result;

    size_t lastBegin = 0;
    for (size_t idx = 1; idx < path.size(); ++idx) {
        if (path[idx] == '/' || path[idx] == '\\') {
            result.push_back(path.substr(lastBegin, idx - lastBegin));
            lastBegin = idx + 1;
        }
    }
    result.push_back(path.substr(lastBegin));
    
    return result;
}


PathsInfo buildPathInfo(lt::file_storage const &fs)
{
    PathsInfo result;
    for (int fIdx = 0; fIdx < fs.num_files(); ++fIdx) {
        std::string const filePath = fs.file_path(fIdx);
        std::vector<std::string> const components = splitPath(filePath);

        std::string curPath = "/";
        for (std::string const &component: components) {
            result.dirs[curPath].insert(component);
            if (curPath.back() != '/') {
                curPath += "/";
            }
            curPath = curPath + component;
        }
        result.files.emplace(curPath, fIdx);
    }
    return result;
}


FuseTorrent::FuseTorrent(
        std::filesystem::path const &torrentFile,
        std::filesystem::path const &targetDirectory):
    m_downloadProgress(
            indicators::option::BarWidth(50),
            indicators::option::Start("["),
            indicators::option::Fill("#"),
            indicators::option::Lead("#"),
            indicators::option::End("]"),
            indicators::option::ShowPercentage(true),
            indicators::option::PostfixText("download progress"),
            indicators::option::ShowElapsedTime(true),
            indicators::option::ShowRemainingTime(true)),
    m_pieceProgress(
            indicators::option::BarWidth(50),
            indicators::option::Start("["),
            indicators::option::Fill("#"),
            indicators::option::Lead("#"),
            indicators::option::End("]"),
            indicators::option::ShowPercentage(true),
            indicators::option::PostfixText("piece request")),
    m_progressBars(m_downloadProgress, m_pieceProgress),
    m_ltSession(),
    m_torrentInfo(torrentFile.generic_string()),
    m_pathMap(buildPathInfo(m_torrentInfo.files())),
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
    m_torrentDownloadThread = std::thread(
            [this]() {
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
    
    if (m_pathMap.dirs.find(path) != m_pathMap.dirs.end()) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    auto it = m_pathMap.files.find(path);
    if (it != m_pathMap.files.end()) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = m_torrentInfo.files().file_size(it->second);
        return 0;
    }

    return -ENOENT;
}


int FuseTorrent::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        fuse_off_t off, fuse_file_info *fi)
{
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    auto it = m_pathMap.dirs.find(path);
    if (it != m_pathMap.dirs.end()) {
        for (std::string const &subPath: it->second) {
            filler(buf, subPath.c_str(), NULL, 0);
        }
    }

    return 0;
}


int FuseTorrent::open(const char *path, fuse_file_info *fi)
{
    auto it = m_pathMap.files.find(path);
    if (it != m_pathMap.files.end()) {
        fi->fh = it->second;
        return 0;
    }
    assert(false);
    return -ENOENT;
}


int FuseTorrent::read(
        const char *path,
        char *buf, size_t const sizeRequested, fuse_off_t const off,
        fuse_file_info *fi)
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
    int64_t const readSize = std::min(
            static_cast<int64_t>(sizeRequested), fileSize - off);

    lt::peer_request const peerRequest = fs.map_file(fIdx, off, readSize);

    PieceData const data = loadWithCache(peerRequest.piece);
    
    int const maxRead = std::min(
            peerRequest.length,
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
        torrentDownloadProgress().set_progress(
                static_cast<size_t>(status.progress * 100));
    }
}


PieceData FuseTorrent::loadWithCache(lt::piece_index_t const pIdx)
{
    auto it = std::find_if(m_pieceCache.begin(), m_pieceCache.end(),
            [pIdx](std::pair<lt::piece_index_t, PieceData> const &p)
            {
                return p.first == pIdx;
            });
    if (it == m_pieceCache.end()) {
        if (m_pieceCache.size() == MAX_CACHE_SIZE) {
            m_pieceCache.pop_front();
        }
        return m_pieceCache.emplace_back(pIdx, loadFromTorrent(pIdx)).second;
    }
    std::pair<lt::piece_index_t, PieceData> const value = *it;
    m_pieceCache.erase(it);
    return m_pieceCache.emplace_back(value).second;
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
    for (lt::piece_index_t pOff = 1; lt::top_priority - pOff != 0 
            && pIdx + pOff < m_torrentInfo.num_pieces(); ++pOff) {
        m_torrentHandle.piece_priority(
                pIdx + pOff, 
                lt::top_priority - pOff);
    }

    pieceProgress().set_progress(0);
    while (!m_torrentHandle.have_piece(pIdx)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<lt::partial_piece_info> const downloadQueue = 
                m_torrentHandle.get_download_queue();
        auto it = std::find_if(
                downloadQueue.begin(), downloadQueue.end(),
                [pIdx](lt::partial_piece_info const &pi)
                {
                    return pi.piece_index == pIdx;
                });
        if (it != downloadQueue.end()) {
            size_t const progress = it->finished * 100 / it->blocks_in_piece;
            pieceProgress().set_progress(progress);
        }
    }
    pieceProgress().set_progress(100);

    return pieceDataFuture.get();
}


indicators::ProgressBar &FuseTorrent::torrentDownloadProgress()
{
    return m_progressBars[0];
}

indicators::ProgressBar &FuseTorrent::pieceProgress()
{
    return m_progressBars[1];
}


FuseTorrent *fuseTorrentFromContext()
{
    return static_cast<FuseTorrent *>(fuse_get_context()->private_data);
}


extern "C"
{

int redirectGetattr(const char *path, struct fuse_stat *stbuf)
{
    return fuseTorrentFromContext()->getattr(path, stbuf);
}


int redirectReaddir(
        const char *path, void *buf, fuse_fill_dir_t filler,
        fuse_off_t off, struct fuse_file_info *fi)
{
    return fuseTorrentFromContext()->readdir(path, buf, filler, off, fi);
}


int redirectOpen(const char *path, struct fuse_file_info *fi)
{
    return fuseTorrentFromContext()->open(path, fi);
}


int redirectRead(
        const char *path, char *buf, size_t size, fuse_off_t off,
        struct fuse_file_info *fi)
{
    return fuseTorrentFromContext()->read(path, buf, size, off, fi);
}


struct fuse_operations initOperations()
{
    struct fuse_operations hello_oper = {};
    hello_oper.getattr = redirectGetattr;
    hello_oper.readdir = redirectReaddir;
    hello_oper.open = redirectOpen;
    hello_oper.read = redirectRead;
    return hello_oper;
}


const struct fuse_operations redirectOperations = initOperations();
}

} // namespace


int downloadTorrentWithFuseMapping(
        std::filesystem::path const &torrentFile,
        std::filesystem::path const &targetDirectory,
        std::filesystem::path const &mappingDirectory)
{
    FuseTorrent fuseTorrent(torrentFile, targetDirectory);

    std::string mappingDirectoryStr = mappingDirectory.generic_string();
    mappingDirectoryStr.push_back('\0');
    int const argc = 3;
    char *argv[4];
    argv[0] = "";
    argv[1] = "-f";
    argv[2] = mappingDirectoryStr.data();
    argv[3] = nullptr;
    return fuse_main(argc, argv, &redirectOperations, &fuseTorrent);
}
