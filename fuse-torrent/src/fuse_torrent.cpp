#include "fuse_torrent.hpp"

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>

#include <indicators/progress_bar.hpp>
#include <indicators/dynamic_progress.hpp>

#include <fuse/fuse.h>

#include <thread>


namespace
{

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

    indicators::ProgressBar &torrentDownloadProgress();

private:
    std::filesystem::path m_mappingDirectory;
    indicators::ProgressBar m_downloadProgress;
    indicators::DynamicProgress<indicators::ProgressBar> m_progressBars;
    lt::session m_ltSession;
    lt::torrent_info m_torrentInfo;
    lt::torrent_handle torrentHandle;
    std::thread m_torrentDownloadThread;
};


FuseTorrent::FuseTorrent(
        std::filesystem::path const &torrentFile,
        std::filesystem::path const &targetDirectory):
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
    torrentHandle(m_ltSession.add_torrent(m_torrentInfo,
            targetDirectory.generic_string(), lt::entry(),
            lt::storage_mode_t::storage_mode_allocate)),
    m_torrentDownloadThread()
{
    m_progressBars.set_option(indicators::option::HideBarWhenComplete(true));
    for (lt::piece_index_t idx = 0; idx < m_torrentInfo.num_pieces(); ++idx) {
        torrentHandle.piece_priority(idx, lt::low_priority);
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
    int res = 0;

    memset(stbuf, 0, sizeof(struct fuse_stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        if (strcmp(path + 1, "hello.txt") == 0) {
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = strlen("hello worldik!");
        } else {
            res = -ENOENT;
        }
    }

    return res;
}


int FuseTorrent::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        fuse_off_t off, fuse_file_info *fi)
{
    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, "hello.txt", NULL, 0);

    return 0;
}


int FuseTorrent::open(const char *path, fuse_file_info *)
{
    return 0;
}


int FuseTorrent::read(const char *path, char *buf, size_t size, fuse_off_t off,
        fuse_file_info *fi)
{
    size_t len;
    (void)fi;
    if (strcmp(path + 1, "hello.txt") != 0) {
        return -ENOENT;
    }

    len = strlen("hello worldik!");
    if (off < len) {
        if (off + size > len)
            size = len - off;
        memcpy(buf, "hello worldik!" + off, size);
    } else {
        size = 0;
    }

    return size;
}


void FuseTorrent::torrentDownloadCycle()
{
    while (true) {
        std::vector<lt::alert *> alerts;
        m_ltSession.pop_alerts(&alerts);

        for (lt::alert const *a: alerts) {
            if (lt::read_piece_alert const *rpa =
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

        lt::torrent_status const status =
                torrentHandle.status(lt::status_flags_t());
        torrentDownloadProgress().set_progress(
                static_cast<size_t>(status.progress * 100));
    }
}


indicators::ProgressBar &FuseTorrent::torrentDownloadProgress()
{
    return m_progressBars[0];
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
    int const argc = 2;
    char *argv[3];
    argv[0] = "";
    argv[1] = mappingDirectoryStr.data();
    argv[2] = nullptr;
    return fuse_main(argc, argv, &redirectOperations, &fuseTorrent);
}
