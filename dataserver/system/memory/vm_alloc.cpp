// vm_alloc.cpp
//
#include "dataserver/system/memory/vm_alloc.h"

#if defined(SDL_OS_WIN32) && SDL_DEBUG
#define SDL_DEBUG_SMALL_MEMORY  0
#else
#define SDL_DEBUG_SMALL_MEMORY  0
#endif

#if SDL_DEBUG_SMALL_MEMORY
#include "dataserver/system/memory/vm_alloc_small.h"
#elif defined(SDL_OS_WIN32)
#include "dataserver/system/memory/vm_alloc_win32.h"
#else
//#include "dataserver/system/memory/vm_alloc_unix.h"
#endif

namespace sdl { namespace db { namespace mmu {

#if SDL_DEBUG_SMALL_MEMORY
class vm_alloc::data_t : public vm_alloc_small {
public:
    using vm_alloc_small::vm_alloc_small;
};
#elif defined(SDL_OS_WIN32)
class vm_alloc::data_t : public vm_alloc_win32 {
public:
    using vm_alloc_win32::vm_alloc_win32;
};
#else
#error not implemented
#endif

//---------------------------------------------

vm_alloc::vm_alloc(uint64 const size)
    : data(new data_t(size))
{
}

vm_alloc::~vm_alloc()
{
}

uint64 vm_alloc::byte_reserved() const
{
    return data->byte_reserved;
}

void * vm_alloc::alloc(uint64 const start, uint64 const size)
{
    return data->alloc(start, size);
}

bool vm_alloc::clear(uint64 const start, uint64 const size)
{
    return data->clear(start, size);
}

#if SDL_DEBUG
namespace {
    class unit_test {
    public:
        unit_test() {
            if (1) {
                enum { page_size = page_head::page_size };
                enum { N = 10 };
                vm_alloc test(page_size * N);
                for (size_t k = 0; k < 2; ++k) {
                for (size_t i = 0; i < N; ++i) {
                    SDL_WARNING(test.alloc(i * page_size, page_size));
                    SDL_WARNING(test.clear(i * page_size, page_size));
                }}
            }
        }
    };
    static unit_test s_test;
}
#endif //#if SDL_DEBUG

} // mmu
} // sdl
} // db
