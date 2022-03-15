#include "PathResolver.hpp"

#include <cassert>


namespace detail
{

PathResolver::PathResolver(lt::file_storage const &fs):
    m_pathInfo(buildPathInfo(fs))
{
}


bool PathResolver::hasDir(const char *path) const
{
    return m_pathInfo.dirs.find(path) != m_pathInfo.dirs.end();
}


int PathResolver::fileIdx(const char *path) const
{
    auto it = m_pathInfo.files.find(path);
    if (it == m_pathInfo.files.end()) {
        return -1;
    }
    return it->second;
}


PathResolver::Files const &PathResolver::dirContent(const char *path) const
{
    assert(hasDir(path));
    return m_pathInfo.dirs.find(path)->second;
}


namespace
{

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

}


PathResolver::PathsInfo PathResolver::buildPathInfo(
        lt::file_storage const &fs)
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

} // namespace detail
// namespace detail
