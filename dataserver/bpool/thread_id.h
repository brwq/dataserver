// thread_id.h
//
#pragma once
#ifndef __SDL_BPOOL_THREAD_ID_H__
#define __SDL_BPOOL_THREAD_ID_H__

#include "dataserver/bpool/block_head.h"
#include "dataserver/common/array.h"
#include "dataserver/common/static_vector.h"
#include <thread>

namespace sdl { namespace db { namespace bpool {

class thread_mask_t : noncopyable {
private:
    enum { node_gigabyte = 8 };
    enum { node_megabyte = gigabyte<node_gigabyte>::value / megabyte<1>::value }; // 8192 MB
    enum { node_block_num = gigabyte<node_gigabyte>::value / pool_limits::block_size }; // 131072 blocks per node
    enum { node_mask_size = node_megabyte / 8 }; // 1 bit per megabyte = 16 blocks
    using node_t = array_t<uint8, node_mask_size>;
    struct node_link;
    using node_p = std::unique_ptr<node_link>;
    struct node_link {
        node_t first;
        node_p second;
        node_link(){
            first.fill_0();
        }
    };
public:
    explicit thread_mask_t(size_t filesize);
private:
    static constexpr size_t init_length(size_t const filesize) {
        return (filesize + gigabyte<node_gigabyte>::value - 1) / 
            gigabyte<node_gigabyte>::value;
    }
private:
    const size_t m_filesize;
    const size_t m_length;
    node_p m_head;
};

class thread_id_t : noncopyable {
    enum { max_thread = pool_limits::max_thread };
public:
    using id_type = std::thread::id;
    using size_bool = std::pair<size_t, bool>;
    explicit thread_id_t(size_t filesize) {}
    static id_type get_id() {
        return std::this_thread::get_id();
    }
    bool empty() const {
        return m_data.empty();
    }
    size_t size() const {
        return m_data.size();
    }
    size_bool insert() {
        return insert(get_id());
    }
    size_bool insert(id_type); // throw if too many threads
    size_bool find(id_type) const;
    size_bool find() const {
        return find(get_id());
    }
    bool erase_id(id_type);
    void erase_pos(size_t);
private:
    using data_type = static_vector<id_type, max_thread + 1>;
    data_type m_data; // sorted for binary search
};

namespace unit {
    struct threadIndex{};
}
typedef quantity<unit::threadIndex, size_t> threadIndex;

}}} // sdl

#endif // __SDL_BPOOL_THREAD_ID_H__
