#ifndef _DETAIL_PATH_RESOLVER_HPP
#define _DETAIL_PATH_RESOLVER_HPP
#include <libtorrent/file_storage.hpp>

#include <map>
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

    // returns -1 for non existent file
    int fileIdx(const char *path) const;

    Files const &dirContent(const char *path) const;

private:
    typedef std::map<std::string, std::set<std::string>> DirsMap;
    typedef std::map<std::string, int> FileToIdxMap;
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
