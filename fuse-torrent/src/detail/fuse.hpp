#ifndef _DETAIL_FUSE_TYPES_HPP
#define _DETAIL_FUSE_TYPES_HPP
// Maps fuse types between fuse and winfsp interfaces
#define FUSE_USE_VERSION 26
#include <fuse/fuse.h>
#ifndef _WIN64

#define fuse_stat stat
typedef off_t fuse_off_t;

#endif // !_WIN64



#endif // _DETAIL_FUSE_TYPES_HPP
