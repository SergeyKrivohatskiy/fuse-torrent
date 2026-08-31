#ifndef _DETAIL_PATH_RESOLVER_HPP
#define _DETAIL_PATH_RESOLVER_HPP
#include <libtorrent/file_storage.hpp>

#include <map>
#include <optional>
#include <string>
#include <set>


namespace detail
{

class PathResolver
{
public:
    typedef std::set<std::string> Files;

public:
    PathResolver(lt::file_storage const &fs);
    
    bool hasDir(const char *path) const;

    std::optional<lt::file_index_t> fileIdx(const char *path) const;

    Files const &dirContent(const char *path) const;

private:
    typedef std::map<std::string, std::set<std::string>> DirsMap;
    typedef std::map<std::string, lt::file_index_t> FileToIdxMap;
    struct PathsInfo
    {
        DirsMap dirs;
        FileToIdxMap files;
    };

private:
    static PathsInfo buildPathInfo(lt::file_storage const &fs);

private:
    PathsInfo m_pathInfo;
};

}
// namespace detail

#endif // _DETAIL_PATH_RESOLVER_HPP
