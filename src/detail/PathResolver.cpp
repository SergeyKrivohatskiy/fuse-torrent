#include "PathResolver.hpp"

#include <cassert>
#include <vector>


namespace detail
{

PathResolver::PathResolver(lt::file_storage const &fs):
    m_pathInfo(buildPathInfo(fs))
{
}


bool PathResolver::hasDir(std::string_view const path) const
{
    return m_pathInfo.dirs.find(path) != m_pathInfo.dirs.end();
}


std::optional<lt::file_index_t> PathResolver::fileIdx(
        std::string_view const path) const
{
    auto const it = m_pathInfo.files.find(path);
    if (it == m_pathInfo.files.end()) {
        return std::nullopt;
    }
    return it->second;
}


PathResolver::Files const &PathResolver::dirContent(
        std::string_view const path) const
{
    assert(hasDir(path));
    return m_pathInfo.dirs.find(path)->second;
}


namespace
{

std::vector<std::string> splitPath(std::string const &path)
{
    std::vector<std::string> result;

    std::size_t lastBegin = 0;
    for (std::size_t idx = 1; idx < path.size(); ++idx) {
        if (path[idx] == '/' || path[idx] == '\\') {
            result.push_back(path.substr(lastBegin, idx - lastBegin));
            lastBegin = idx + 1;
        }
    }
    result.push_back(path.substr(lastBegin));

    return result;
}

} // namespace


PathResolver::PathsInfo PathResolver::buildPathInfo(lt::file_storage const &fs)
{
    PathsInfo result;
    for (lt::file_index_t const fIdx: fs.file_range()) {
        std::string const filePath = fs.file_path(fIdx);
        std::vector<std::string> const components = splitPath(filePath);

        std::string curPath = "/";
        for (std::string const &component: components) {
            result.dirs[curPath].insert(component);
            if (curPath.back() != '/') {
                curPath += "/";
            }
            curPath += component;
        }
        result.files.emplace(curPath, fIdx);
    }
    return result;
}

} // namespace detail
