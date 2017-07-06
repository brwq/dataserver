// page_pool.cpp
//
#include "dataserver/system/page_pool.h"

#if SDL_TEST_PAGE_POOL
namespace sdl { namespace db { namespace pp {
#if 1
#define SDL_PAGE_ASSERT(...)   SDL_ASSERT(__VA_ARGS__)
#else
#define SDL_PAGE_ASSERT(...)   ((void)0)
#endif

#if SDL_PAGE_POOL_STAT
thread_local PagePool::unique_page_stat
PagePool::thread_page_stat;
#endif

//https://msdn.microsoft.com/en-us/library/windows/desktop/aa364218(v=vs.85).aspx
// The amount of I/O performance improvement that file data caching offers depends on 
// the size of the file data block being read or written. 
// When large blocks of file data are read and written, 
// it is more likely that disk reads and writes will be necessary to finish the I/O operation. 
// I/O performance will be increasingly impaired as more of this kind of I/O operation occurs.
// In these situations, caching can be turned off. 
// This is done at the time the file is opened by passing FILE_FLAG_NO_BUFFERING as a value for the dwFlagsAndAttributes parameter of CreateFile.
// When caching is disabled, all read and write operations directly access the physical disk. 
// However, the file metadata may still be cached.
// To flush the metadata to disk, use the FlushFileBuffers function.

#if 0 //defined(SDL_OS_WIN32)
PagePoolFile_win32::PagePoolFile_win32(const std::string & fname)
{
    /*HANDLE WINAPI CreateFile(
      _In_     LPCTSTR               lpFileName,
      _In_     DWORD                 dwDesiredAccess,
      _In_     DWORD                 dwShareMode,
      _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
      _In_     DWORD                 dwCreationDisposition,
      _In_     DWORD                 dwFlagsAndAttributes,
      _In_opt_ HANDLE                hTemplateFile
    );*/
}

PagePoolFile_win32::~PagePoolFile_win32()
{
}

inline
bool PagePoolFile_win32::is_open() const {
    return false;
}

inline
void PagePoolFile_win32::read_all(char * const dest){
    SDL_ASSERT(dest);
}

inline
void PagePoolFile_win32::read(char * const dest, const size_t offset, const size_t size) {
    SDL_ASSERT(dest);
    SDL_ASSERT(size && !(size % page_head::page_size));
    SDL_ASSERT(offset + size <= filesize());
}
#else
PagePoolFile::PagePoolFile(const std::string & fname)
    : m_file(fname, std::ifstream::in | std::ifstream::binary)
{
    if (m_file.is_open()) {
        m_file.seekg(0, std::ios_base::end);
        m_filesize = m_file.tellg();
        m_file.seekg(0, std::ios_base::beg);
    }
}

inline
bool PagePoolFile::is_open() const {
    return m_file.is_open();
}

inline
void PagePoolFile::read_all(char * const dest){
    SDL_ASSERT(dest);
    m_file.seekg(0, std::ios_base::beg);
    m_file.read(dest, m_filesize);
    m_file.seekg(0, std::ios_base::beg);
}

inline
void PagePoolFile::read(char * const dest, const size_t offset, const size_t size) {
    SDL_ASSERT(dest);
    SDL_ASSERT(size && !(size % page_head::page_size));
    SDL_ASSERT(offset + size <= filesize());
    m_file.seekg(offset, std::ios_base::beg);
    m_file.read(dest, size);
}
#endif

//--------------------------------------------------------------

PagePool::PagePool(const std::string & fname)
    : m_file(fname)
{
    SDL_TRACE_FUNCTION;
    static_assert(is_power_two(max_page), "");
    static_assert(is_power_two(slot_size), "");
    static_assert(power_of<slot_size>::value == 16, "");
    static_assert(gigabyte<1>::value / page_size == 131072, "");
    static_assert(gigabyte<5>::value / page_size == 655360, "");
    static_assert(gigabyte<1>::value / slot_size == 16384, "");
    static_assert(gigabyte<5>::value / slot_size == 81920, "");
    throw_error_if_not<this_error>(m_file.is_open(), "file not found");
    m.filesize = m_file.filesize();
    m.page_count = m.filesize / page_size;
    m.slot_count = (m.filesize + slot_size - 1) / slot_size;
    SDL_PAGE_ASSERT((slot_page_num != 8) || (m.slot_count * slot_page_num == m.page_count));
    if (valid_filesize(m.filesize)) {
        m_slot_commit.resize(m.slot_count);
        m_alloc.reset(new char[m.filesize]);
        throw_error_if_not<this_error>(is_open(), "bad alloc");
#if SDL_PAGE_POOL_LOAD_ALL
        load_all();
#endif
    }
    else {
        throw_error<this_error>("bad alloc size");
    }
}

#if SDL_DEBUG 
bool PagePool::check_page(page_head const * const head, const pageIndex pageId) {
    if (1) { // only for allocated page
        SDL_ASSERT(head->valid_checksum() || !head->data.tornBits);
        SDL_ASSERT(head->data.pageId.pageId == pageId.value());
    }
    return true;
}
#endif

bool PagePool::valid_filesize(const size_t filesize)
{
    if (filesize > slot_size) {
        SDL_PAGE_ASSERT(!(filesize % page_size));
        SDL_PAGE_ASSERT((slot_page_num != 8) || !(filesize % slot_size));
        return !(filesize % page_size);
    }
    return false;
}

void PagePool::load_all()
{
    SDL_TRACE(__FUNCTION__, " [", m.filesize, "] byte");
    SDL_UTILITY_SCOPE_TIMER_SEC(timer, "load_all seconds = ");
    m_file.read_all(m_alloc.get());
    m_slot_commit.assign(m.slot_count, true);
}

page_head const *
PagePool::load_page(pageIndex const index) {
    const size_t pageId = index.value(); // uint32 => size_t
    SDL_PAGE_ASSERT(pageId < m.page_count);
#if SDL_PAGE_POOL_STAT
    if (thread_page_stat) {
        thread_page_stat->load_page.insert((uint32)pageId);
        thread_page_stat->load_page_request++;
    }
#endif
    if (pageId < page_count()) {
        lock_guard lock(m_mutex);
        return load_page_nolock(index);
    }
    SDL_TRACE("page not found: ", pageId);
    throw_error<this_error>("page not found");
    return nullptr;
}

page_head const *
PagePool::load_page_nolock(pageIndex const index) {
    const size_t pageId = index.value(); // uint32 => size_t
    const size_t slotId = pageId / slot_page_num;
    SDL_PAGE_ASSERT(pageId < m.page_count);
    SDL_PAGE_ASSERT(slotId < m.slot_count);
#if SDL_PAGE_POOL_STAT
    if (thread_page_stat) {
        thread_page_stat->load_slot.insert((uint32)slotId);
    }
#endif
    char * const page_ptr = m_alloc.get() + pageId * page_size;
    if (!m_slot_commit[slotId]) { //FIXME: should use sequential access to file pages
        char * const slot_ptr = m_alloc.get() + slotId * slot_size;
        if (slotId == m.last_slot()) {
            SDL_PAGE_ASSERT(slot_ptr + m.last_slot_size() == m_alloc.get() + m.filesize);
            m_file.read(slot_ptr, slotId * slot_size, m.last_slot_size());
        }
        else {
            m_file.read(slot_ptr, slotId * slot_size, slot_size);
        }
        m_slot_commit[slotId] = true;
    }
    page_head const * const head = reinterpret_cast<page_head const *>(page_ptr);
    SDL_PAGE_ASSERT(check_page(head, index));
    return head;
}

#if SDL_PAGE_POOL_STAT
void PagePool::page_stat_t::trace() const {
    SDL_TRACE("load_page = ", load_page.size(), 
        "/", load_page_request,
        "/", load_slot.size(),
        "\nload_slot:");
    load_slot.trace();
}
#endif

} // pp
} // db
} // sdl
#endif // SDL_TEST_PAGE_POOL