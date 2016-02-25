// index_tree.h
//
#ifndef __SDL_SYSTEM_INDEX_TREE_H__
#define __SDL_SYSTEM_INDEX_TREE_H__

#pragma once

#include "datapage.h"
#include "page_iterator.h"

namespace sdl { namespace db {

class database;

class index_tree_base: noncopyable {
    database * const db;       
    page_head const * const root;
protected:
    class index_access { // level index scan
        friend index_tree_base;
        index_tree_base const * parent;
        page_head const * head;
        size_t slot_index;
    public:
        explicit index_access(index_tree_base const * p, page_head const * h, size_t i = 0)
            : parent(p), head(h), slot_index(i)
        {
            SDL_ASSERT(parent && head);
            SDL_ASSERT(slot_index <= slot_array::size(head));
        }
        bool operator == (index_access const & x) const {
            return (head == x.head) && (slot_index == x.slot_index);
        }
        template<class index_page_row>
        index_page_row const * dereference() const {
            SDL_ASSERT(head->data.pminlen == sizeof(index_page_row));
            return datapage_t<index_page_row>(head)[slot_index];
        }
    };
    void load_next(index_access&);
    void load_prev(index_access&);
    bool is_end(index_access const &);
    bool is_begin(index_access const &);    
    index_access get_begin();
    index_access get_end();
    explicit index_tree_base(database *, page_head const *);
};

template<class T>
class index_tree: index_tree_base {
public:
    using key_type = T;
    using row_type = index_page_row_t<T>;
    using row_reference = row_type const &;
public:
    using iterator = page_iterator<index_tree, index_access>;
    friend iterator;

    explicit index_tree(database * p, page_head const * h)
        : index_tree_base(p, h)
    {}
    iterator begin() {
        return iterator(this, get_begin());
    }
    iterator end() {
        return iterator(this, get_end());
    }
    template<class fun_type>
    void for_reverse(fun_type fun);
private:
    row_type const * dereference(index_access const & p) {
        return p.dereference<row_type>();
    }
};

template<class T>
template<class fun_type>
void index_tree<T>::for_reverse(fun_type fun)
{
    iterator last = this->begin();
    iterator it = this->end();
    if (it != last) {
        do {
            --it;
            fun(*(*it));
        } while (it != last);
    }
}

template<scalartype::type v>
using index_tree_t = index_tree<typename index_key<v>::type>;

} // db
} // sdl

#endif // __SDL_SYSTEM_INDEX_TREE_H__
