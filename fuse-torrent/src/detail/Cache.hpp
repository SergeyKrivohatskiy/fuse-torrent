#ifndef _DETAIL_CACHE_HPP
#define _DETAIL_CACHE_HPP
#include <list>
#include <cassert>


namespace detail
{

template<class Key, class Value, size_t CAPACITY>
class Cache
{
public:
    Cache();

    Value *get(Key key);
    Value &insert(Key key, Value value);

private:
    std::list<std::pair<Key, Value>> m_data;
};


#endif // _DETAIL_CACHE_HPP


template<class Key, class Value, size_t CAPACITY>
Cache<Key, Value, CAPACITY>::Cache():
    m_data()
{
}


template<class Key, class Value, size_t CAPACITY>
Value *Cache<Key, Value, CAPACITY>::get(Key key)
{
    auto it = std::find_if(m_data.begin(), m_data.end(),
            [key](std::pair<Key, Value> const &p)
            {
                return p.first == key;
            });
    if (it == m_data.end()) {
        return nullptr;
    }
    
    m_data.push_back(std::move(*it));
    m_data.erase(it);
    
    return &m_data.back().second;
}


template<class Key, class Value, size_t CAPACITY>
Value &Cache<Key, Value, CAPACITY>::insert(Key key, Value value)
{
    assert(!get(key));
    if (m_data.size() == CAPACITY) {
        m_data.pop_front();
    }
    return m_data.emplace_back(std::move(key), std::move(value)).second;
}

}
// namespace detail
