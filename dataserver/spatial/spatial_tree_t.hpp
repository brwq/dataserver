// spatial_tree_t.hpp
//
#pragma once
#ifndef __SDL_SPATIAL_SPATIAL_TREE_T_HPP__
#define __SDL_SPATIAL_SPATIAL_TREE_T_HPP__

namespace sdl { namespace db {

template<typename KEY_TYPE>
spatial_tree_t<KEY_TYPE>::spatial_tree_t(database const * const p, 
                           page_head const * const h, 
                           shared_primary_key const & pk0,
                           sysidxstats_row const * const idx)
    : this_db(p), cluster_root(h), idxstat(idx)
{
    A_STATIC_ASSERT_TYPE(scalartype_t<scalartype::t_bigint>, int64);
    SDL_ASSERT(this_db && cluster_root && idxstat);
    SDL_ASSERT(1 == pk0->size()); //FIXME: current implementation
    if (pk0 && pk0->is_index() && is_index(cluster_root) && (1 == pk0->size())) {
        SDL_ASSERT(pk0->first_type() == key_to_scalartype<pk0_type>::value);
        m_min_page = load_leaf_page(true);
        m_max_page = load_leaf_page(false); 
    }
    else {
        throw_error<spatial_tree_error>("bad index");
    }
    SDL_ASSERT(is_str_empty(spatial_datapage::name()));
    SDL_ASSERT(find_page(min_cell()));
    SDL_ASSERT(find_page(max_cell()));
}

template<typename KEY_TYPE>
bool spatial_tree_t<KEY_TYPE>::is_index(page_head const * const h)
{
    if (h && h->is_index() && slot_array::size(h)) {
        SDL_ASSERT(h->data.pminlen == sizeof(spatial_tree_row));
        return true;
    }
    return false;
}

template<typename KEY_TYPE>
bool spatial_tree_t<KEY_TYPE>::is_data(page_head const * const h)
{
    if (h && h->is_data() && slot_array::size(h)) {
        SDL_ASSERT(h->data.pminlen == sizeof(spatial_page_row));
        return true;
    }
    return false;
}

template<typename KEY_TYPE> inline
void spatial_tree_t<KEY_TYPE>::datapage_access::load_next(state_type & p) const
{
    p = fwd::load_next_head(tree->this_db, p);
}

template<typename KEY_TYPE> inline
void spatial_tree_t<KEY_TYPE>::datapage_access::load_prev(state_type & p) const
{
    if (p) {
        SDL_ASSERT(p != tree->min_page());
        SDL_ASSERT(p->data.prevPage);
        p = fwd::load_prev_head(tree->this_db, p);
    }
    else {
        p = tree->max_page();
    }
}

template<typename KEY_TYPE>
page_head const * spatial_tree_t<KEY_TYPE>::load_leaf_page(bool const begin) const
{
    page_head const * head = cluster_root;
    while (1) {
        SDL_ASSERT(is_index(head));
        const spatial_index page(head);
        const auto row = begin ? page.front() : page.back();
        if (auto next = fwd::load_page_head(this_db, row->data.page)) {
            if (next->is_index()) {
                head = next;
            }
            else {
                SDL_ASSERT(is_data(next));
                SDL_ASSERT(!begin || !head->data.prevPage);
                SDL_ASSERT(begin || !head->data.nextPage);
                SDL_ASSERT(!begin || !next->data.prevPage);
                SDL_ASSERT(begin || !next->data.nextPage);
                return next;
            }
        }
        else {
            SDL_ASSERT(0);
            break;
        }
    }
    throw_error<spatial_tree_error>("bad index");
    return nullptr;
}

template<typename KEY_TYPE> inline
page_head const * spatial_tree_t<KEY_TYPE>::min_page() const
{
    SDL_ASSERT(m_min_page);
    return m_min_page;
}

template<typename KEY_TYPE> inline
page_head const * spatial_tree_t<KEY_TYPE>::max_page() const
{
    SDL_ASSERT(m_max_page);
    return m_max_page;
}

template<typename KEY_TYPE>
typename spatial_tree_t<KEY_TYPE>::spatial_page_row const *
spatial_tree_t<KEY_TYPE>::min_page_row() const
{
    if (auto const p = min_page()) {
        const spatial_datapage page(p);
        if (!page.empty()) {
            return page.front();
        }
    }
    SDL_ASSERT(0);
    return{};
}

template<typename KEY_TYPE>
typename spatial_tree_t<KEY_TYPE>::spatial_page_row const *
spatial_tree_t<KEY_TYPE>::max_page_row() const
{
    if (auto const p = max_page()) {
        const spatial_datapage page(p);
        if (!page.empty()) {
            return page.back();
        }
    }
    SDL_ASSERT(0);
    return{};
}

template<typename KEY_TYPE>
spatial_cell spatial_tree_t<KEY_TYPE>::min_cell() const
{
    if (auto const p = min_page_row()) {
        return p->data.cell_id;
    }
    SDL_ASSERT(0);
    return{};
}

template<typename KEY_TYPE>
spatial_cell spatial_tree_t<KEY_TYPE>::max_cell() const
{
    if (auto const p = max_page_row()) {
        return p->data.cell_id;
    }
    SDL_ASSERT(0);
    return{};
}

template<typename KEY_TYPE>
size_t spatial_tree_t<KEY_TYPE>::find_slot(spatial_index const & data, cell_ref cell_id)
{
    spatial_tree_row const * const null = data.prevPage() ? nullptr : data.front();
    size_t i = data.lower_bound([&cell_id, null](spatial_tree_row const * const x) {
        if (x == null)
            return true;
        return x->data.key.cell_id < cell_id;
    });
    SDL_ASSERT(i <= data.size());
    if (i < data.size()) {
        if (i && (cell_id < data[i]->data.key.cell_id)) {
            --i;
        }
        return i;
    }
    SDL_ASSERT(i);
    return i - 1; // last slot
}

template<typename KEY_TYPE>
pageFileID spatial_tree_t<KEY_TYPE>::find_page(cell_ref cell_id) const
{
    SDL_ASSERT(cell_id);
    page_head const * head = cluster_root;
    while (1) {
        SDL_ASSERT(is_index(head));
        const spatial_index data(head);
        auto const & id = data[find_slot(data, cell_id)]->data.page;
        if (auto const next = fwd::load_page_head(this_db, id)) {
            if (next->is_index()) {
                head = next;
                continue;
            }
            if (next->is_data()) {
                SDL_ASSERT(id);
                return id;
            }
            SDL_ASSERT(0);
        }
        break;
    }
    SDL_ASSERT(0);
    return{};
}

template<typename KEY_TYPE> inline
bool spatial_tree_t<KEY_TYPE>::intersect(spatial_page_row const * p, cell_ref c)
{
    SDL_ASSERT(p);
    return p ? p->data.cell_id.intersect(c) : false;
}

template<typename KEY_TYPE> inline
bool spatial_tree_t<KEY_TYPE>::is_front_intersect(page_head const * const h, cell_ref cell_id)
{
    SDL_ASSERT(h->is_data());
    return spatial_datapage(h).front()->data.cell_id.intersect(cell_id);
}

template<typename KEY_TYPE> inline
bool spatial_tree_t<KEY_TYPE>::is_back_intersect(page_head const * const h, cell_ref cell_id)
{
    SDL_ASSERT(h->is_data());
    return spatial_datapage(h).back()->data.cell_id.intersect(cell_id);
}

template<typename KEY_TYPE>
page_head const * spatial_tree_t<KEY_TYPE>::page_lower_bound(cell_ref cell_id) const
{
    auto const id = find_page(cell_id);
    if (page_head const * h = fwd::load_page_head(this_db, id)) {
        while (is_front_intersect(h, cell_id)) {
            if (auto h2 = fwd::load_prev_head(this_db, h)) {
                if (is_back_intersect(h2, cell_id)) {
                    h = h2;
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        }
        SDL_ASSERT(is_data(h));
        return h;
    }
    SDL_ASSERT(0);
    return nullptr;
}

template<typename KEY_TYPE>
typename spatial_tree_t<KEY_TYPE>::spatial_page_row const *
spatial_tree_t<KEY_TYPE>::load_page_row(recordID const & pos) const
{
    if (page_head const * const h = fwd::load_page_head(this_db, pos.id)) {
        SDL_ASSERT(is_data(h) && (pos.slot < slot_array(h).size()));
        return spatial_datapage(h)[pos.slot];
    }
    SDL_ASSERT(!pos);
    return nullptr;
}

template<typename KEY_TYPE>
recordID spatial_tree_t<KEY_TYPE>::find_cell(cell_ref cell_id) const
{
    SDL_ASSERT(cell_id);
    if (page_head const * const h = page_lower_bound(cell_id)) {
        const spatial_datapage data(h);
        if (data) {
            size_t const slot = data.lower_bound(
                [&cell_id](spatial_page_row const * const row) {
                return (row->data.cell_id < cell_id) && !row->data.cell_id.intersect(cell_id);
            });
            if (slot == data.size()) {
                return {};
            }
            return recordID::init(h->data.pageId, slot);
        }
        SDL_ASSERT(0);
    }
    return {};
}

template<typename KEY_TYPE>
template<class fun_type>
break_or_continue spatial_tree_t<KEY_TYPE>::for_cell(cell_ref c1, fun_type && fun) const // try optimize
{
    using depth_t = spatial_cell::id_type;
    A_STATIC_ASSERT_TYPE(uint8, depth_t);
    SDL_ASSERT(c1);
    spatial_cell c2{};
    depth_t const max_depth = c1.data.depth;
    recordID it; // uninitialized
    spatial_page_row const * last = nullptr;
    for (depth_t i = 1; i <= max_depth; ++i) {
        c2.data.depth = i;
        c2.data.id.cell[i - 1] = c1.data.id.cell[i - 1];
        if ((it = find_cell(c2))) {
            spatial_page_row const * p = load_page_row(it);
            if (p != last) {
                if (!last || (last->data.cell_id < p->data.cell_id)) {
                    while (p->data.cell_id.intersect(c1)) {
                        if (make_break_or_continue(fun(p)) == bc::break_) {
                            return bc::break_;
                        }
                        last = p;
                        if ((it = fwd::load_next_record(this_db, it))) {
                            p = load_page_row(it);
                            SDL_ASSERT(p);
                        }
                        else {
                            break;
                        }
                    }
                }
            }
        }
    }
    return bc::continue_;
}

#if 0 // old
template<typename KEY_TYPE>
template<class fun_type>
break_or_continue spatial_tree_t<KEY_TYPE>::for_range(spatial_point const & p, Meters const radius, fun_type && fun) const
{
    SDL_TRACE_DEBUG_2("for_range(", p.latitude, ",",  p.longitude, ",", radius.value(), ")");
    interval_cell ic;
    transform::cell_range(ic, p, radius);
    SDL_TRACE_DEBUG_2("cell_count = ", ic.size(), ", contains = ", ic.contains());
    return ic.for_each([this, &fun](spatial_cell const & cell){
        return this->for_cell(cell, fun);
    });
}

template<typename KEY_TYPE>
template<class fun_type>
break_or_continue spatial_tree_t<KEY_TYPE>::for_rect(spatial_rect const & rc, fun_type && fun) const
{
    SDL_TRACE_DEBUG_2("for_rect(", rc.min_lat, ",",  rc.min_lon, ",", rc.max_lat, ",", rc.max_lon, ")");
    interval_cell ic;
    transform::cell_rect(ic, rc);
    SDL_TRACE_DEBUG_2("cell_count = ", ic.size(), ", contains = ", ic.contains());
    return ic.for_each([this, &fun](spatial_cell const & cell){
        return this->for_cell(cell, fun);
    });
}
#endif

#if SDL_DEBUG

namespace todo {

    inline int number_of_1(uint64 n) {
        int count = 0;
        while (n) {
            ++count;
            n = (n - 1) & n;
        }
        return count;
    }

    class sparse_set : noncopyable {
        using pk0_type = int64;
        using umask_t = uint64;
        static constexpr umask_t seg_size = sizeof(umask_t) * 8;
        static constexpr umask_t seg_mask = seg_size - 1;
        static_assert(sizeof(pk0_type) == sizeof(umask_t), "");
        static_assert(seg_size == 64, "");
        using map_type = std::unordered_map<size_t, umask_t>;
        map_type m_mask;
        size_t m_size = 0;
    public:
        sparse_set() = default;
        size_t size() const {
            return m_size;
        }
        size_t contains() const {
            return m_mask.size();
        }
        void clear() {
            m_mask.clear();
            m_size = 0;
        }
        bool insert(pk0_type const value) {
            const size_t seg = (umask_t)(value) / seg_size;
            const size_t bit = (umask_t)(value) & seg_mask;
            const umask_t test = umask_t(1) << bit;
            SDL_ASSERT(test < (umask_t)(-1));
            SDL_ASSERT(bit == ((umask_t)value % seg_size));
            SDL_ASSERT(value == make_pk0(seg, bit));
            umask_t & slot = m_mask[seg];
            if (slot & test) {
                return false;
            }
            slot |= test;
            ++m_size;
#if 0 //SDL_DEBUG > 1
            {
                size_t test_size = 0;
                for_each([&test_size](pk0_type v){
                    ++test_size;
                    return bc::continue_;
                });
                SDL_ASSERT(test_size == m_size);
            }
#endif
            return true;
        }
    private:
        static pk0_type make_pk0(const size_t seg, const size_t bit) {
            const umask_t base = (umask_t)seg * seg_size;
            const umask_t uvalue = base + bit;
            const pk0_type value = (pk0_type)(uvalue);
            return value;
        }
        template<class fun_type>
        break_or_continue for_each(fun_type && fun) const;
    };

    template<class fun_type>
    break_or_continue
    sparse_set::for_each(fun_type && fun) const {
        auto const last = m_mask.end();
        for(auto it = m_mask.begin(); it != last; ++it) {
            const umask_t base = it->first * seg_size;
            umask_t slot = it->second;
            SDL_ASSERT(slot);
            for (size_t bit = 0; slot; ++bit) {
                SDL_ASSERT(bit < seg_size);
                if (slot & 1) {
                    const umask_t uvalue = base + bit;
                    const pk0_type value = (pk0_type)(uvalue);
                    SDL_ASSERT(value == make_pk0(it->first, bit));
                    if (is_break(fun(value))) {
                        return bc::break_;
                    }
                }
                slot >>= 1;
            }
        }
        return bc::continue_;
    }

    template<typename pk0_type>
    struct pk0_type_set {
        using type = interval_set<pk0_type>;
    };
    template<> struct pk0_type_set<int64> {
        using type = sparse_set;
    };

} // todo

template<typename KEY_TYPE>
template<class fun_type> break_or_continue 
spatial_tree_t<KEY_TYPE>::for_range(spatial_point const & p, Meters const radius, fun_type && fun) const
{
    SDL_TRACE_DEBUG_2("for_range(", p.latitude, ",",  p.longitude, ",", radius.value(), ")");
    typename todo::pk0_type_set<pk0_type>::type set_pk0; // check processed records
    //interval_set<pk0_type> set_pk0; // check processed records
    size_t cell_count = 0;
    SDL_UTILITY_SCOPE_EXIT([&set_pk0, &cell_count]{
        SDL_TRACE("for_range::set_pk0 size = ", set_pk0.size(),
            " contains = ", set_pk0.contains(),
            " cell_count = ", cell_count);
    });
    auto function = [this, &fun, &set_pk0, &cell_count](spatial_cell cell) {
        return this->for_cell(cell, [&fun, &set_pk0, &cell_count](spatial_page_row const * const row) {
            ++cell_count;
            if (set_pk0.insert(row->data.pk0)) {
                return make_break_or_continue(fun(row));
            }
            return bc::continue_;
        });
    };
    return transform::cell_range(function_cell_t<decltype(function)>(std::move(function)), p, radius);    
}

template<typename KEY_TYPE>
template<class fun_type> break_or_continue
spatial_tree_t<KEY_TYPE>::for_rect(spatial_rect const & rc, fun_type && fun) const
{
    SDL_TRACE_DEBUG_2("for_rect(", rc.min_lat, ",",  rc.min_lon, ",", rc.max_lat, ",", rc.max_lon, ")");
    typename todo::pk0_type_set<pk0_type>::type set_pk0; // check processed records
    //interval_set<pk0_type> set_pk0; // check processed records
    size_t cell_count = 0;
    SDL_UTILITY_SCOPE_EXIT([&set_pk0, &cell_count]{
        SDL_TRACE("for_rect::set_pk0 size = ", set_pk0.size(), 
            " contains = ", set_pk0.contains(),
            " cell_count = ", cell_count);
    });
    auto function = [this, &fun, &set_pk0, &cell_count](spatial_cell cell){
        return this->for_cell(cell, [&fun, &set_pk0, &cell_count](spatial_page_row const * const row) {
            ++cell_count;
            if (set_pk0.insert(row->data.pk0)) {
                return make_break_or_continue(fun(row));
            }
            return bc::continue_;
        });
    };
    return transform::cell_rect(function_cell_t<decltype(function)>(std::move(function)), rc);
}
#else
template<typename KEY_TYPE>
template<class fun_type>
break_or_continue spatial_tree_t<KEY_TYPE>::for_range(spatial_point const & p, Meters const radius, fun_type && fun) const
{
    SDL_TRACE_DEBUG_2("for_range(", p.latitude, ",",  p.longitude, ",", radius.value(), ")");
    //interval_set<pk0_type> m_pk0; // check processed records
    auto function = [this, &fun](spatial_cell cell) {
        return this->for_cell(cell, fun);
    };
    return transform::cell_range(
        function_cell_t<decltype(function)>(std::move(function)),
        p, radius);    
}

template<typename KEY_TYPE>
template<class fun_type>
break_or_continue spatial_tree_t<KEY_TYPE>::for_rect(spatial_rect const & rc, fun_type && fun) const
{
    SDL_TRACE_DEBUG_2("for_rect(", rc.min_lat, ",",  rc.min_lon, ",", rc.max_lat, ",", rc.max_lon, ")");
    auto function = [this, &fun](spatial_cell cell){
        return this->for_cell(cell, fun);
    };
    return transform::cell_rect(
        function_cell_t<decltype(function)>(std::move(function)), rc);
}
#endif

template<typename KEY_TYPE>
template<class fun_type>
break_or_continue spatial_tree_t<KEY_TYPE>::full_globe(fun_type && fun) const
{
    page_head const * h = min_page();
    while (h) {
        const spatial_datapage data(h);
        for (auto const p : data) {
            A_STATIC_CHECK_TYPE(spatial_page_row const * const, p);
            if (make_break_or_continue(fun(p)) == bc::break_) {
                return bc::break_;
            }
        }
        h = fwd::load_next_head(this_db, h);
    }
    return bc::continue_;
}

} // db
} // sdl

#endif // __SDL_SPATIAL_SPATIAL_TREE_T_HPP__