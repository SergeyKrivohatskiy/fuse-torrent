#include "fuse_torrent.hpp"

#include "detail/FuseTorrent.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>


namespace
{

detail::FuseTorrent *fuseTorrentFromContext()
{
    return static_cast<detail::FuseTorrent *>(fuse_get_context()->private_data);
}

} // namespace


extern "C"
{

static void *fuseTorrentRedirectInit(fuse_conn_info *conn)
{
    return fuseTorrentFromContext()->init(conn);
}


static int fuseTorrentRedirectGetattr(char const *path, fuse_stat *stbuf)
{
    return fuseTorrentFromContext()->getattr(path, stbuf);
}


static int fuseTorrentRedirectReaddir(char const *path, void *buf,
        fuse_fill_dir_t filler, fuse_off_t off, fuse_file_info *fi)
{
    return fuseTorrentFromContext()->readdir(path, buf, filler, off, fi);
}


static int fuseTorrentRedirectOpen(char const *path, fuse_file_info *fi)
{
    return fuseTorrentFromContext()->open(path, fi);
}


static int fuseTorrentRedirectRead(char const *path, char *buf,
        std::size_t size, fuse_off_t off, fuse_file_info *fi)
{
    return fuseTorrentFromContext()->read(path, buf, size, off, fi);
}

}


namespace
{

fuse_operations initOperations()
{
    fuse_operations operations = {};
    operations.init = fuseTorrentRedirectInit;
    operations.getattr = fuseTorrentRedirectGetattr;
    operations.readdir = fuseTorrentRedirectReaddir;
    operations.open = fuseTorrentRedirectOpen;
    operations.read = fuseTorrentRedirectRead;
    return operations;
}


fuse_operations const redirectOperations = initOperations();

} // namespace


int downloadTorrentWithFuseMapping(
        std::filesystem::path const &torrentFile,
        std::filesystem::path const &targetDirectory,
        std::filesystem::path const &mappingDirectory)
{
    detail::FuseTorrent fuseTorrent(torrentFile, targetDirectory);

    char program[] = "";
    char foreground[] = "-f";

    std::string const mountPointPath = mappingDirectory.generic_string();
    std::vector<char> mountPoint(mountPointPath.begin(), mountPointPath.end());
    mountPoint.push_back('\0');

    std::array<char *, 4> argv = {program, mountPoint.data(), foreground,
                                  nullptr};

#ifndef _WIN64
    std::filesystem::create_directories(mappingDirectory);
#endif // !_WIN64
    int const ret = fuse_main(static_cast<int>(argv.size() - 1), argv.data(),
            &redirectOperations, &fuseTorrent);
#ifndef _WIN64
    std::filesystem::remove_all(mappingDirectory);
#endif // !_WIN64
    return ret;
}
