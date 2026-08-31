#ifndef FUSE_TORRENT_DETAIL_FUSE_HPP
#define FUSE_TORRENT_DETAIL_FUSE_HPP
#define FUSE_USE_VERSION 26
#include <fuse.h>

#ifndef _WIN64
#include <sys/stat.h>
#include <sys/types.h>

using fuse_stat = struct stat;
using fuse_off_t = off_t;
#endif // !_WIN64

#endif // FUSE_TORRENT_DETAIL_FUSE_HPP
