#ifndef FUSE_TORRENT_DETAIL_PATH_RESOLVER_HPP
#define FUSE_TORRENT_DETAIL_PATH_RESOLVER_HPP
#include <libtorrent/file_storage.hpp>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>


namespace detail
{

class PathResolver
{
public:
    using Files = std::set<std::string>;

public:
    explicit PathResolver(lt::file_storage const &fs);

    [[nodiscard]] bool hasDir(std::string_view path) const;

    [[nodiscard]] std::optional<lt::file_index_t> fileIdx(
            std::string_view path) const;

    [[nodiscard]] Files const &dirContent(std::string_view path) const;

private:
    using DirsMap = std::map<std::string, Files, std::less<>>;
    using FileToIdxMap = std::map<std::string, lt::file_index_t, std::less<>>;
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

} // namespace detail

#endif // FUSE_TORRENT_DETAIL_PATH_RESOLVER_HPP
