// page_bpool.inl
//
#pragma once
#ifndef __SDL_BPOOL_PAGE_BPOOL_INL__
#define __SDL_BPOOL_PAGE_BPOOL_INL__

namespace sdl { namespace db { namespace bpool {

inline bool page_bpool::is_open() const
{
    return m_file.is_open() && m_alloc.base();
}

inline void const * page_bpool::start_address() const
{
    return m_alloc.base();
}

inline size_t page_bpool::page_count() const
{
    return info.page_count;
}

inline page_head *
page_bpool::get_block_page(char * const block_adr, size_t const i) {
    SDL_ASSERT(block_adr);
    SDL_ASSERT(i < pool_limits::block_page_num);
    return reinterpret_cast<page_head *>(block_adr + i * pool_limits::page_size);
}

inline block_head *
page_bpool::get_block_head(page_head * const p) {
    static_assert(sizeof(block_head) == page_head::reserved_size, "");
    uint8 * const b = p->data.reserved;
    return reinterpret_cast<block_head *>(b);
}

inline block_head *
page_bpool::first_block_head(char * const block_adr) {
    SDL_ASSERT(block_adr);
    return get_block_head(reinterpret_cast<page_head *>(block_adr));
}

inline block_head *
page_bpool::first_block_head(block32 const blockId) const {
    SDL_ASSERT(blockId);
    block_head * const p = first_block_head(m_alloc.get_block(blockId));
    SDL_ASSERT(p->blockId == blockId);
    SDL_ASSERT(p->realBlock);
    return p;
}

inline page_head const *
page_bpool::zero_block_page(pageIndex const pageId) {
    SDL_ASSERT(pageId.value() < pool_limits::block_page_num);
    char * const page_adr = m_alloc.base() + page_bit(pageId) * pool_limits::page_size;
    return reinterpret_cast<page_head *>(page_adr);
}

inline void page_bpool::read_block_from_file(char * const block_adr, size_t const blockId) {
     m_file.read(block_adr, blockId * pool_limits::block_size, info.block_size_in_bytes(blockId)); 
}

inline uint32 page_bpool::pageAccessTime() const {
#if 0
    return unix_time();
#else
    return ++m_pageAccessTime;
#endif
}

#if SDL_DEBUG
inline bool page_bpool::assert_page(pageIndex id) {
    return lock_page(id) != nullptr;
}
#endif

//----------------------------------------------------------------

inline block_head * 
block_list_t::first_block_head(block32 const blockId) const {
    return m_p->first_block_head(blockId);
}

}}} // sdl

#endif // __SDL_BPOOL_PAGE_BPOOL_INL__
