// vm_unix.cpp
//
#include "dataserver/bpool/vm_unix.h"
#include "dataserver/filesys/mmap64_unix.h" // mmap, mmap64
#include <numeric>

#if defined(SDL_OS_UNIX)
    #if !defined(MAP_ANONYMOUS)
        #define MAP_ANONYMOUS MAP_ANON
    #endif
#endif

namespace sdl { namespace db { namespace bpool {

vm_unix_base::vm_unix_base(size_t const size)
    : byte_reserved(size)
    , page_reserved(size / page_size)
    , block_reserved(size / block_size)
    , arena_reserved(size / arena_size)
{
    A_STATIC_ASSERT_64_BIT;
    SDL_ASSERT(size && !(size % arena_size));
}

vm_unix_new::vm_unix_new(size_t const size, vm_commited const f)
    : vm_unix_base(get_arena_size(size) * arena_size)
    , m_arena(arena_reserved)
    , m_free_arena_list{}
    , m_mixed_arena_list{}
{
    A_STATIC_ASSERT_64_BIT;
    SDL_ASSERT(size && !(size % block_size));
    SDL_ASSERT(page_reserved <= max_page);
    SDL_ASSERT(block_reserved <= max_block);
    SDL_ASSERT(byte_reserved == arena_reserved * arena_size);
    A_STATIC_ASSERT_IS_POD(arena_index);
    A_STATIC_ASSERT_IS_POD(block_t);
    A_STATIC_ASSERT_IS_POD(arena_t);
    static_assert(sizeof(arena_index) == 4, "");
    static_assert(sizeof(block_t) == 4, "");
    static_assert(sizeof(arena_t) == 14, "");
    static_assert(block_size * arena_block_num == arena_size, "");
    static_assert(get_arena_size(gigabyte<1>::value) == 1024, "");
    static_assert(get_arena_size(terabyte<1>::value) == 1024*1024, ""); // 1048576
    static_assert(arena_t::mask_all == 0xFFFF, "");
    if (is_commited(f)) {
        size_t i = 0;
        for (auto & x : m_arena) {
            alloc_arena(x, i++);
        }
        SDL_ASSERT(i == arena_reserved);
        if (use_sort_arena) {
            m_sort_adr.resize(arena_reserved);
            std::iota(m_sort_adr.begin(), m_sort_adr.end(), 0);
            sort_adr();
        }
    }
    else {
        if (use_sort_arena) {
            m_sort_adr.reserve(1024);
        }
    }
    SDL_ASSERT(!m_free_arena_list);
    SDL_ASSERT(!m_mixed_arena_list);
    SDL_ASSERT(!m_arena_brk);
}

vm_unix_new::~vm_unix_new()
{
    for (arena_t & x : m_arena) {
        if (x.arena_adr)
            sys_free_arena(x.arena_adr);
    }
}

void vm_unix_new::sort_adr() {
    if (use_sort_arena) {
        SDL_ASSERT(m_sort_adr.size() == m_alloc_arena_count);
        std::sort(m_sort_adr.begin(), m_sort_adr.end(), [this](uint32 x, uint32 y){
            return m_arena[x].arena_adr < m_arena[y].arena_adr;
        });
    }
}

vm_unix_new::sort_adr_t::iterator
vm_unix_new::find_sort_adr(arena32 const index) {
    if (use_sort_arena) {
        SDL_ASSERT(m_sort_adr.size() == m_alloc_arena_count);
        const auto it = std::lower_bound(m_sort_adr.begin(), m_sort_adr.end(), index,
            [this](arena32 x, arena32 y){
                return m_arena[x].arena_adr < m_arena[y].arena_adr;
        });
        SDL_ASSERT(it != m_sort_adr.end());
        SDL_ASSERT(*it == index);
        return it;
    }
    SDL_ASSERT(0);
    return m_sort_adr.end();
}

size_t vm_unix_new::find_arena(char const * const p) const
{
    SDL_ASSERT(p != nullptr);
    SDL_ASSERT(m_arena_brk && (m_arena_brk <= arena_reserved));
    SDL_ASSERT(m_alloc_arena_count);
    if (use_sort_arena) {
        SDL_ASSERT(m_sort_adr.size() == m_alloc_arena_count);
        auto it = std::lower_bound(m_sort_adr.begin(), m_sort_adr.end(), p,
            [this](arena32 x, char const * arena_adr){
                return m_arena[x].arena_adr < arena_adr;
        });
        if (it != m_sort_adr.end()) {
            if (m_arena[*it].arena_adr == p) {
                return *it;
            }
            if (it != m_sort_adr.begin()) {
                --it;
                char const * const found = m_arena[*it].arena_adr;
                SDL_ASSERT(found < p);
                if ((found <= p) && (p < (found + arena_size))) {
                    return *it;
                }
            }
            SDL_ASSERT(0);
        }
        else {
            char const * const last = m_arena[m_sort_adr.back()].arena_adr;
            SDL_ASSERT(last < p);
            if ((last <= p) && (p < (last + arena_size))) {
                return m_sort_adr.back();
            }
        }
        SDL_ASSERT(0);
        return arena_reserved;
    }
    else { // not optimized (linear search)
        size_t result = 0;
        for (auto const & x : m_arena) {
            SDL_ASSERT(result < m_arena_brk);
            if (x.arena_adr) {
                if ((p >= x.arena_adr) && (p < (x.arena_adr + arena_size))) {
                    return result;
                }
            }
            ++result;
        }
        SDL_ASSERT(result == arena_reserved);
        SDL_ASSERT(0);
        return result;
    }
}

void vm_unix_new::alloc_arena(arena_t & x, const size_t i) {
    (void)i;
    SDL_ASSERT(&x == &m_arena[i]);
    SDL_TRACE_DEBUG_2("alloc_arena[", i, "]"); 
    if (!x.arena_adr) {
        x.arena_adr = sys_alloc_arena(); // throw if failed
        SDL_ASSERT(debug_zero_arena(x));
        ++m_alloc_arena_count;
        SDL_ASSERT(m_alloc_arena_count <= arena_reserved);
        SDL_TRACE_DEBUG_2("alloc_arena_count = ", m_alloc_arena_count);
        if (use_sort_arena) {
            m_sort_adr.push_back(static_cast<sort_adr_t::value_type>(i));
            sort_adr();
        }
    }
    SDL_ASSERT(x.arena_adr && !x.block_mask);
}

void vm_unix_new::free_arena(arena_t & x, const size_t i) {
    (void)i;
    SDL_ASSERT(&x == &m_arena[i]);
    SDL_TRACE_DEBUG_2("free_arena[", i, "]"); 
    SDL_ASSERT(x.arena_adr && x.empty());
    if (x.arena_adr) {
        if (use_sort_arena) {
            m_sort_adr.erase(find_sort_adr(static_cast<sort_adr_t::value_type>(i)));
        }
        sys_free_arena(x.arena_adr);
        x.arena_adr = nullptr;
        SDL_ASSERT(m_alloc_arena_count);
        --m_alloc_arena_count;
        SDL_TRACE_DEBUG_2("alloc_arena_count = ", m_alloc_arena_count);
    }
}

size_t vm_unix_new::count_free_arena_list() const {
    size_t result = 0;
    for(auto p = m_free_arena_list; p; ++result) {
        const auto & x = m_arena[p.index()];
        SDL_ASSERT(!x.arena_adr && x.empty());
        p = x.next_arena;
    }
    SDL_ASSERT(result <= m_arena_brk);
    return result;
}

size_t vm_unix_new::count_mixed_arena_list() const {
    size_t result = 0;
    for(auto p = m_mixed_arena_list; p; ++result) {
        const auto & x = m_arena[p.index()];
        SDL_ASSERT(x.arena_adr && x.mixed());
        p = x.next_arena;
    }
    SDL_ASSERT(result <= m_arena_brk);
    return result;
}

char * vm_unix_new::sys_alloc_arena() {
#if defined(SDL_OS_UNIX)
    void * const p = mmap64_t::call(nullptr, arena_size, 
        PROT_READ | PROT_WRITE // the desired memory protection of the mapping
        , MAP_PRIVATE | MAP_ANONYMOUS // private copy-on-write mapping. The mapping is not backed by any file
        ,-1 // file descriptor
        , 0 // offset must be a multiple of the page size as returned by sysconf(_SC_PAGE_SIZE)
    );
    throw_error_if_t<vm_unix_new>(!p, "mmap64_t failed");
    return reinterpret_cast<char *>(p);
#else
    char * const p = reinterpret_cast<char *>(std::malloc(arena_size));
    throw_error_if_t<vm_unix_new>(!p, "bad malloc");
    return p;
#endif
}

bool vm_unix_new::sys_free_arena(char * const p) {
    SDL_ASSERT(p);
#if defined(SDL_OS_UNIX)
    if (::munmap(p, arena_size)) {
        SDL_ASSERT(!"munmap");
        return false;
    }
#else
    std::free(p);
#endif
    return true;
}

char * vm_unix_new::alloc_next_arena_block() 
{
    SDL_ASSERT(m_arena_brk < arena_reserved);
    if (m_arena_brk == arena_reserved) {
        SDL_ASSERT(0);
        return nullptr;
    }
    const size_t i = m_arena_brk++;
    arena_t & x = m_arena[i];
    alloc_arena(x, i);
    x.set_block<0>();
    SDL_ASSERT(x.set_block_count() == 1);
    add_to_mixed_arena_list(x, i);
    SDL_TRACE_DEBUG_2("alloc_next_arena_block = ", i);
    return x.arena_adr;
}

char * vm_unix_new::alloc_block_without_count()
{
    SDL_ASSERT(m_arena_brk <= arena_reserved);
    if (!m_arena_brk) {
        SDL_ASSERT(!m_mixed_arena_list);
        SDL_ASSERT(!m_free_arena_list);
        return alloc_next_arena_block();
    }
    if (m_mixed_arena_list) { // use mixed first
        const size_t i = m_mixed_arena_list.index();
        arena_t & x = m_arena[i];
        SDL_ASSERT(x.arena_adr && x.mixed()); 
        const size_t index = x.find_free_block();
        x.set_block(index);
        char * const p = x.arena_adr + (index << power_of<block_size>::value);
        SDL_ASSERT(find_arena(p) == i);
        if (x.full()) {
            m_mixed_arena_list = x.next_arena; // can be null
            x.next_arena.set_null();
        }
        return p;
    }
    if (m_free_arena_list) { // use m_free_arena_list first
        const size_t i = m_free_arena_list.index();
        arena_t & x = m_arena[i];
        SDL_ASSERT(x.empty() && !x.arena_adr);
        m_free_arena_list = x.next_arena; // can be null
        x.next_arena.set_null();
        alloc_arena(x, i);
        x.set_block<0>();
        SDL_ASSERT(x.set_block_count() == 1);
        add_to_mixed_arena_list(x, i);
        return x.arena_adr;
    }
    SDL_ASSERT(m_arena_brk);
    SDL_ASSERT(!m_mixed_arena_list);
    SDL_DEBUG_CPP(const arena_t & test = m_arena[m_arena_brk - 1]);
    SDL_ASSERT(test.full() && test.arena_adr);
    return alloc_next_arena_block();
}

bool vm_unix_new::remove_from_mixed_arena_list(size_t const i)
{
    if (!m_mixed_arena_list) {
        return false;
    }
    if (m_mixed_arena_list.index() == i) {
        arena_t & x = m_arena[i];
        SDL_ASSERT(x.arena_adr);
        m_mixed_arena_list = x.next_arena;
        x.next_arena.set_null();
        return true;
    }
    arena_index prev = m_mixed_arena_list;
    arena_index p = m_arena[prev.index()].next_arena;
    while (p) {
        SDL_ASSERT(prev);
        arena_t & x = m_arena[p.index()];
        SDL_ASSERT(x.arena_adr);
        if (p.index() == i) {
            m_arena[prev.index()].next_arena = x.next_arena;
            x.next_arena.set_null();
            return true;
        }
        SDL_ASSERT(x.mixed() && x.arena_adr);
        prev = p;
        p = x.next_arena;
    }
    SDL_ASSERT(0);
    return false;
}

bool vm_unix_new::release_without_count(char * const start)
{
    SDL_ASSERT(m_arena_brk && (m_arena_brk <= arena_reserved));
    SDL_ASSERT(start);
    const size_t i = find_arena(start);
    if (i < arena_reserved) {
        arena_t & x = m_arena[i];
        const size_t offset = start - x.arena_adr;
        SDL_ASSERT(!(offset % block_size));
        SDL_ASSERT(offset < arena_size);
        const size_t j = (offset >> power_of<block_size>::value);
        SDL_ASSERT(x.is_block(j));
        x.clr_block(j);
        if (x.empty()) { // release area, add to m_free_area_list
            SDL_ASSERT(!x.block_mask);
            remove_from_mixed_arena_list(i);
            free_arena(x, i);
            add_to_free_arena_list(x, i);
            SDL_ASSERT_DEBUG_2(find_free_arena_list(i));
            SDL_ASSERT_DEBUG_2(!find_mixed_arena_list(i));
            return true;
        }
        SDL_ASSERT(x.mixed());
        if (1 == x.free_block_count()) { // add to m_mixed_arena_list
            SDL_ASSERT_DEBUG_2(!find_mixed_arena_list(i));
            if (m_mixed_arena_list) {
                x.next_arena.set_index(m_mixed_arena_list.index());
            }
            else {
                x.next_arena.set_null();
            }
            m_mixed_arena_list.set_index(i);
        }
        SDL_ASSERT_DEBUG_2(find_mixed_arena_list(i));
        return true;
    }
    SDL_ASSERT(0);
    return false;
}

#if SDL_DEBUG
bool vm_unix_new::find_block_in_list(arena_index p, size_t const i) const {
    while (p) {
        if (p.index() == i) {
            return true;
        }
        p = m_arena[p.index()].next_arena;
    }
    return false;
}
bool vm_unix_new::find_free_arena_list(size_t const i) const {
    auto p = m_free_arena_list;
    while (p) {
        if (p.index() == i) {
            SDL_ASSERT(!m_arena[i].arena_adr);
            return true;
        }
        const arena_t & x = m_arena[p.index()];
        SDL_ASSERT(!x.arena_adr);
        p = x.next_arena;
    }
    return false;
}
bool vm_unix_new::find_mixed_arena_list(size_t const i) const {
    auto p = m_mixed_arena_list;
    while (p) {
        if (p.index() == i) {
            SDL_ASSERT(m_arena[i].mixed());
            SDL_ASSERT(m_arena[i].arena_adr);
            return true;
        }
        const arena_t & x = m_arena[p.index()];
        SDL_ASSERT(x.arena_adr && x.mixed());
        p = x.next_arena;
    }
    return false;
}
#endif

vm_unix_new::block32
vm_unix_new::get_block_id(char const * const p) const
{
    SDL_ASSERT(p);
    const size_t i = find_arena(p);
    if (i < arena_reserved) {
        const arena_t & x = m_arena[i];
        const size_t offset = p - x.arena_adr;
        SDL_ASSERT(!(offset % block_size));
        SDL_ASSERT(offset < arena_size);
        const size_t j = (offset >> power_of<block_size>::value);
        SDL_ASSERT(x.is_block(j));
        SDL_ASSERT(block_t::init(i, j).value < block_reserved);
        return block_t::init(i, j).value;
    }
    SDL_ASSERT(0);
    return block_index::invalid_block32;
}

char * vm_unix_new::get_block(block32 const id) const
{
    SDL_ASSERT(m_arena_brk && (m_arena_brk <= arena_reserved));
    const block_t b = block_t::init_id(id);
    SDL_ASSERT(b.d.arenaId < arena_reserved);
    SDL_ASSERT(b.d.arenaId < m_arena_brk);
    const arena_t & a = m_arena[b.d.arenaId];
    if (a.arena_adr) {
        if (a.is_block(b.d.index)) {
            static_assert(power_of<block_size>::value == 16, "");
            return a.arena_adr + (size_t(b.d.index) << power_of<block_size>::value);
        }
    }
    SDL_ASSERT(0);
    return nullptr;
}

size_t vm_unix_new::defragment(can_move_block_fun && can_move_block)
{
    SDL_ASSERT(can_move_block);
    return 0; // not implemented
    if (!m_mixed_arena_list || m_arena[m_mixed_arena_list.index()].next_arena.is_null()) {
        return 0;
    }
    using arena_block = std::pair<arena32, uint8>; // <index, set_block_count>
    std::vector<arena_block> mixed(count_mixed_arena_list());
    {
        SDL_ASSERT(mixed.size() > 1);
        arena_index p = m_mixed_arena_list;
        for (auto & val : mixed) {
            SDL_ASSERT(p);
            const auto & x = m_arena[p.index()];
            SDL_ASSERT(x.arena_adr && x.mixed());
            val.first = static_cast<arena32>(p.index());
            val.second = static_cast<uint8>(x.set_block_count());
            SDL_ASSERT(val.second && (val.second < arena_block_num));
            p = x.next_arena;
        }
        std::sort(mixed.begin(), mixed.end(), [](arena_block const & x, arena_block const & y){
            return x.second < y.second;
        });
    }
    arena_block * lh = mixed.data();
    arena_block * rh = lh + mixed.size() - 1;
    SDL_ASSERT(lh < rh);
    while (lh < rh) {
        SDL_ASSERT(lh->second < rh->second);
        auto & x = m_arena[lh->first];
        auto & y = m_arena[rh->first];
        SDL_ASSERT(x.set_block_count() < y.set_block_count());
        SDL_ASSERT(x.arena_adr && y.arena_adr);
        while (lh->second && (rh->second < arena_block_num)) {
            SDL_ASSERT(!x.empty() && !y.full());
            const size_t lb = x.find_set_block();
            const size_t rb = y.find_free_block();
            const block_t xb = block_t::init(lh->first, lb);
            const block_t yb = block_t::init(rh->first, rb);
            if (can_move_block(xb.value, yb.value)) {
                SDL_DEBUG_CPP(const auto p1 = get_block(xb.value));
                SDL_DEBUG_CPP(auto p2 = get_block(yb.value));
                memcpy(get_block(yb.value), get_block(xb.value), block_size);
                break; //FIXME: 
            }
            --(lh->second);
            ++(rh->second);
            break;
        }
        break;
    }
    return 0;
}

//---------------------------------------------------------------

#if SDL_DEBUG
namespace {
class unit_test {
    static void test(vm_commited);
public:
    unit_test() {
        test(vm_commited::false_);
        test(vm_commited::true_);
        SDL_TRACE_FUNCTION;
    }
};

void unit_test::test(vm_commited const flag) {
    if (1) {
        using T = vm_unix_new;
        T test(T::arena_size * 2 + T::block_size * 3, flag);
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < test.block_reserved; ++i) {
                if (char * const p = test.alloc_block()) {
                    const auto b = test.get_block_id(p);
                    SDL_ASSERT(b < test.block_reserved);
                    SDL_ASSERT(p == test.get_block(b));
                    if (0 == j) {
                        SDL_ASSERT(test.release(p));
                    }
                }
            }
        }
    }
    if (1) {
        using T = vm_unix_new;
        enum { test_size = T::arena_size * 2 + T::block_size * 3 };
        T test(test_size, flag);
        SDL_ASSERT(test.byte_reserved >= test_size);
        std::vector<char *> block_adr;
        for (size_t i = 0; i < (test_size / T::block_size); ++i) {
            block_adr.push_back(test.alloc_block());
            const size_t t2 = test.count_mixed_arena_list();
            if ((i + 1) % T::arena_block_num) {
                SDL_ASSERT(t2 == 1);
            }
            else {
                SDL_ASSERT(t2 == 0);
            }
        }
        {
            const size_t t0 = test.arena_brk();
            const size_t t1 = test.count_free_arena_list();
            const size_t t2 = test.count_mixed_arena_list();
            SDL_ASSERT(t0 == 3);
            SDL_ASSERT(t1 == 0);
            SDL_ASSERT(t2 == 1);
        }
        for (size_t i = 0; i < (test.arena_block_num * 2 + 1); ++i) {
            if (test.release(block_adr[i])) {
                block_adr[i] = nullptr;
            }
        }
        {
            const size_t t0 = test.arena_brk();
            const size_t t1 = test.count_free_arena_list();
            const size_t t2 = test.count_mixed_arena_list();
            SDL_ASSERT(t0 == 3);
            SDL_ASSERT(t1 == 2);
            SDL_ASSERT(t2 == 1);
        }
        {
            vm_unix_new::arena_t test {};
            test.block_mask = 0x5555;
            SDL_ASSERT(test.set_block_count() == 8);
        }
    }
}

static unit_test s_test;
}
#endif // SDL_DEBUG
}}} // db
