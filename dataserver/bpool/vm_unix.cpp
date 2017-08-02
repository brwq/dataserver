// vm_unix.cpp
//
#include "dataserver/bpool/vm_unix.h"
#include "dataserver/filesys/mmap64_unix.h" // mmap, mmap64

#if defined(SDL_OS_UNIX)
    #if !defined(MAP_ANONYMOUS)
        #define MAP_ANONYMOUS MAP_ANON
    #endif
#endif

#if defined(SDL_OS_UNIX) || SDL_DEBUG
namespace sdl { namespace db { namespace bpool {

char * vm_unix_old::init_vm_alloc(size_t const size, vm_commited const f) {
    if (size && !(size % block_size)) {
#if defined(SDL_OS_UNIX)
        void * const base = mmap64_t::call(nullptr, size, 
            is_commited(f) ? (PROT_READ | PROT_WRITE) : // the desired memory protection of the mapping
                PROT_NONE // pages may not be accessed
            , MAP_PRIVATE | MAP_ANONYMOUS // private copy-on-write mapping. The mapping is not backed by any file
            ,-1 // file descriptor
            , 0 // offset must be a multiple of the page size as returned by sysconf(_SC_PAGE_SIZE)
        );
        throw_error_if_t<vm_unix>(!base, "mmap64_t failed");
        return reinterpret_cast<char *>(base);
#endif // SDL_OS_UNIX
    }
    throw_error_t<vm_unix_old>("init_vm_alloc failed");
    return nullptr;
}

vm_unix_old::vm_unix_old(size_t const size, vm_commited const f)
    : byte_reserved(size)
    , page_reserved(size / page_size)
    , block_reserved(size / block_size)
    , m_base_address(init_vm_alloc(size, f))
{
    A_STATIC_ASSERT_64_BIT;
    SDL_ASSERT(size && !(size % block_size));
    SDL_ASSERT(page_reserved <= max_page);
    SDL_ASSERT(block_reserved <= max_block);
    SDL_ASSERT(is_open());
    SDL_DEBUG_CPP(d_block_commit.resize(block_reserved, is_commited(f)));
    SDL_TRACE("vm_unix is_commited = ", is_commited(f));
}

vm_unix_old::~vm_unix_old()
{
    if (m_base_address) {
#if defined(SDL_OS_UNIX)
        if (::munmap(m_base_address, byte_reserved)) {
            SDL_ASSERT(!"munmap");
        }
#endif
    }
}

#if SDL_DEBUG
bool vm_unix_old::assert_address(char const * const start, size_t const size) const {
    SDL_ASSERT(m_base_address <= start);
    SDL_ASSERT(!((start - m_base_address) % block_size));
    SDL_ASSERT(size && !(size % block_size));
    SDL_ASSERT(start + size <= end_address());
    return true;
}

size_t vm_unix_old::count_alloc_block() const {
    return std::count(d_block_commit.begin(), d_block_commit.end(), true);
}
#endif

char * vm_unix_old::alloc(char * const start, const size_t size)
{
    SDL_ASSERT(assert_address(start, size));
#if SDL_DEBUG
    {
        if (0) {
            SDL_TRACE("vm_unix::alloc = ", (start - m_base_address) / block_size);
        }
        size_t b = (start - m_base_address) / block_size;
        const size_t endb = b + (size + block_size - 1) / block_size;
        SDL_ASSERT(b < endb);
        SDL_ASSERT(endb <= block_reserved);
        for (; b < endb; ++b) {
            SDL_ASSERT(!d_block_commit[b]);
            d_block_commit[b] = true;
        }
    }
#endif
#if defined(SDL_OS_UNIX)
    if (::mprotect(start, size, PROT_READ | PROT_WRITE)) {
        SDL_ASSERT(!"mprotect");
        throw_error_t<vm_unix>("mprotect PROT_READ|PROT_WRITE failed");
    }
#endif
    return start;
}

// start and size must be aligned to blocks
bool vm_unix_old::release(char * const start, const size_t size)
{
    SDL_ASSERT(assert_address(start, size));
#if SDL_DEBUG
    {
        if (0) {
            SDL_TRACE("vm_unix::release = ", (start - m_base_address) / block_size, " N = ", size / block_size);
        }
        size_t b = (start - m_base_address) / block_size;
        const size_t endb = b + (size + block_size - 1) / block_size;
        SDL_ASSERT(b < endb);
        SDL_ASSERT(endb <= block_reserved);
        for (; b < endb; ++b) {
            SDL_ASSERT(d_block_commit[b]);
            d_block_commit[b] = false;
        }
    }
#endif
#if defined(SDL_OS_UNIX)
    if (::mprotect(start, size, PROT_NONE)) {
        SDL_ASSERT(!"mprotect");
        throw_error_t<vm_unix>("mprotect PROT_NONE failed");
    }
#endif
    return true;
}

#if SDL_DEBUG
namespace {
class unit_test {
public:
    unit_test() {
        if (0) {
            using T = vm_unix_old;
            T test(T::block_size * 3, vm_commited::false_);
            for (size_t i = 0; i < test.block_reserved; ++i) {
                auto const p = test.base_address() + i * T::block_size;
                SDL_ASSERT(test.alloc(p, T::block_size));
            }
            SDL_ASSERT(test.release(test.base_address(), test.byte_reserved));
        }
    }
};
static unit_test s_test;
}
#endif // SDL_DEBUG
}}} // db
#endif // SDL_OS_UNIX
