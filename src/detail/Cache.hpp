#ifndef FUSE_TORRENT_DETAIL_CACHE_HPP
#define FUSE_TORRENT_DETAIL_CACHE_HPP
#include <algorithm>
#include <cstddef>
#include <list>
#include <utility>


namespace detail
{

template<class Key, class Value, std::size_t CAPACITY>
class Cache
{
public:
    Value *get(Key key);
    Value &insert(Key key, Value value);

private:
    std::list<std::pair<Key, Value>> m_data;
};


template<class Key, class Value, std::size_t CAPACITY>
Value *Cache<Key, Value, CAPACITY>::get(Key const key)
{
    auto const it = std::ranges::find_if(m_data,
            [key](std::pair<Key, Value> const &entry)
            {
                return entry.first == key;
            });
    if (it == m_data.end()) {
        return nullptr;
    }

    m_data.splice(m_data.end(), m_data, it);

    return &m_data.back().second;
}


template<class Key, class Value, std::size_t CAPACITY>
Value &Cache<Key, Value, CAPACITY>::insert(Key key, Value value)
{
    if (Value *const present = get(key)) {
        *present = std::move(value);
        return *present;
    }
    if (m_data.size() == CAPACITY) {
        m_data.pop_front();
    }
    return m_data.emplace_back(std::move(key), std::move(value)).second;
}

} // namespace detail

#endif // FUSE_TORRENT_DETAIL_CACHE_HPP
