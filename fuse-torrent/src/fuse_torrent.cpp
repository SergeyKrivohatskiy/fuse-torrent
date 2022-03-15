#include "fuse_torrent.hpp"

#include "detail/FuseTorrent.hpp"


namespace
{

detail::FuseTorrent *fuseTorrentFromContext()
{
    return static_cast<detail::FuseTorrent *>(fuse_get_context()->private_data);
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

}
// namespace


int downloadTorrentWithFuseMapping(
        std::filesystem::path const &torrentFile,
        std::filesystem::path const &targetDirectory,
        std::filesystem::path const &mappingDirectory)
{
    detail::FuseTorrent fuseTorrent(torrentFile, targetDirectory);
    
    char arg0[] = "";
    std::string arg1 = mappingDirectory.generic_string();
    arg1.push_back('\0');
    char arg2[] = "-f";

    int const argc = 3;
    char *argv[4];
    argv[0] = arg0;
    argv[1] = arg1.data();
    argv[2] = arg2;
    argv[3] = nullptr;
    
#ifndef _WIN64
    std::filesystem::create_directories(mappingDirectory);
#endif // _WIN64
    int const ret = fuse_main(argc, argv, &redirectOperations, &fuseTorrent);
#ifndef _WIN64
    std::filesystem::remove_all(mappingDirectory);
#endif // _WIN64
    return ret;
}
