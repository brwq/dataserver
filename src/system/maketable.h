// maketable.h
//
#ifndef __SDL_SYSTEM_MAKETABLE_H__
#define __SDL_SYSTEM_MAKETABLE_H__

#pragma once

#include "database.h"
#include "maketable_meta.h"

namespace sdl { namespace db { namespace make {

template<class META>
class make_base_table: public noncopyable {
    using TYPE_LIST = typename META::type_list;
    database * const m_db;
    shared_usertable const schema;
public:
    enum { col_size = TL::Length<TYPE_LIST>::value };
    enum { col_fixed = meta::IsFixed<TYPE_LIST>::value };
    const usertable & ut() const { return *schema; }
protected:
    make_base_table(database * p, shared_usertable const & s): m_db(p), schema(s) {
        SDL_ASSERT(m_db && schema);
    }
    ~make_base_table() = default;
private:
    template<class T> // T = col::
    using ret_type = typename T::ret_type;

    template<class T> // T = col::
    using val_type = typename T::val_type;

    template<class R, class V>
    static R get_empty(identity<R>, identity<V>) {
        static const V val{};
        return val;
    }
    static var_mem get_empty(identity<var_mem>, identity<var_mem>) {
        return {};
    }
    template<class T> // T = col::
    static ret_type<T> get_empty() {
        using R = ret_type<T>;
        using V = val_type<T>;
        return get_empty(identity<R>(), identity<V>());
    }
    template<class T> // T = col::
    static ret_type<T> fixed_val(row_head const * const p, meta::is_fixed<1>) { // is fixed 
        static_assert(T::fixed, "");
        return p->fixed_val<typename T::val_type>(T::offset);
    }
    template<class T> // T = col::
    ret_type<T> fixed_val(row_head const * const p, meta::is_fixed<0>) const { // is variable 
        static_assert(!T::fixed, "");
        return m_db->get_variable(p, T::offset, T::type);
    }
    template<class T> // T = col::
    using col_index = TL::IndexOf<TYPE_LIST, T>;

    template<size_t i> 
    using col_t = typename TL::TypeAt<TYPE_LIST, i>::Result;

    template<size_t i>
    using col_ret_type = typename TL::TypeAt<TYPE_LIST, i>::Result::ret_type;

    template<class T> // T = col::
    ret_type<T> get_value(row_head const * const p, identity<T>, meta::is_fixed<0>) const {
        if (null_bitmap(p)[col_index<T>::value]) {
            return get_empty<T>();
        }
        static_assert(!T::fixed, "");
        return fixed_val<T>(p, meta::is_fixed<T::fixed>());
    }
    template<class T> // T = col::
    static ret_type<T> get_value(row_head const * const p, identity<T>, meta::is_fixed<1>) {
        if (null_bitmap(p)[col_index<T>::value]) {
            return get_empty<T>();
        }
        static_assert(T::fixed, "");
        return fixed_val<T>(p, meta::is_fixed<T::fixed>());
    }
private:
    class null_record {
    protected:
        row_head const * row = nullptr;
        null_record(row_head const * h): row(h) {
            SDL_ASSERT(row);
        }
        null_record() = default;
        ~null_record() = default;
    public:
        bool is_null(size_t const i) const {   
            return null_bitmap(this->row)[i];
        }
        template<size_t i>
        bool is_null() const {
            static_assert(i < col_size, "");
            return is_null(i);
        }
        template<class T> // T = col::
        bool is_null() const {
            return is_null<col_index<T>::value>();
        }
        explicit operator bool() const {
            return this->row != nullptr;
        }
    };
    template<class this_table, bool is_fixed>
    class base_record_t;

    template<class this_table>
    class base_record_t<this_table, true> : public null_record {
    protected:
        base_record_t(this_table const *, row_head const * h): null_record(h) {
            static_assert(col_fixed, "");
        }
        base_record_t() = default;
        ~base_record_t() = default;
    public:
        template<class T> // T = col::
        ret_type<T> val() const {
            return make_base_table::get_value(this->row, identity<T>(), meta::is_fixed<T::fixed>());
        }
    protected:
        template<class T> // T = col::
        ret_type<T> get_value(identity<T>) const {
            return make_base_table::get_value(this->row, identity<T>(), meta::is_fixed<T::fixed>());
        }
    };
    template<class this_table>
    class base_record_t<this_table, false> : public null_record {
        this_table const * table = nullptr;
    protected:
        base_record_t(this_table const * p, row_head const * h): null_record(h), table(p) {
            SDL_ASSERT(table);
            static_assert(!col_fixed, "");
        }
        base_record_t() = default;
        ~base_record_t() = default;

        template<class T> // T = col::
        ret_type<T> get_value(identity<T>) const {
            return table->get_value(this->row, identity<T>(), meta::is_fixed<T::fixed>());
        }
    public:
        template<class T> // T = col::
        ret_type<T> val() const {
            return get_value(identity<T>());
        }
    };
protected:
    template<class this_table>
    class base_record : public base_record_t<this_table, col_fixed> {
        using base = base_record_t<this_table, col_fixed>;
    protected:
        base_record(this_table const * p, row_head const * h): base(p, h) {}
        ~base_record() = default;
        base_record() = default;
    public:
        template<size_t i>
        col_ret_type<i> get() const {
            static_assert(i < col_size, "");
            return this->get_value(identity<col_t<i>>());
        }
        template<class T> // T = col::
        std::string type_col() const {
            return type_col<T>(meta::is_fixed<T::fixed>());
        }
    private:
        template<class T> // T = col::
        std::string type_col(meta::is_fixed<1>) const {
            return to_string::type(this->get_value(identity<T>()));
        }
        template<class T> // T = col::
        std::string type_col(meta::is_fixed<0>) const {
            return to_string::dump_mem(this->get_value(identity<T>()));
        }
    };
protected:
    template<class this_table, class record_type>
    class base_access: noncopyable {
        using record_iterator = datatable::datarow_iterator;
        this_table const * const table;
        detached_datarow _datarow;
    public:
        using iterator = forward_iterator<base_access, record_iterator>;
        base_access(this_table const * p, database * const d, shared_usertable const & s)
            : table(p), _datarow(d, s)
        {
            SDL_ASSERT(table);
            SDL_ASSERT(s->get_id() == this_table::id);
            SDL_ASSERT(s->name() == this_table::name());
        }
        iterator begin() {
            return iterator(this, _datarow.begin());
        }
        iterator end() {
            return iterator(this, _datarow.end());
        }
    private:
        friend iterator;
        record_type dereference(record_iterator const & it) {
            A_STATIC_CHECK_TYPE(row_head const *, *it);
            return record_type(table, *it);
        }
        void load_next(record_iterator & it) {
            ++it;
        }
    };
};

template<class this_table, class record>
class make_query: noncopyable {
public:
    using cluster_index = typename this_table::cluster_index;
    using cluster_key = typename meta::cluster_key<cluster_index>::type;
    using vector_record = std::vector<record>;
public:
    explicit make_query(this_table * p) : table(*p) {
        SDL_ASSERT(meta::check_cluster_index<cluster_index>());
    }
    template<class fun_type>
    void scan_if(fun_type fun) {
        for (auto p : table) {
            if (!fun(p)) {
                break;
            }
        }
    }
    //FIXME: SELECT select_list [ ORDER BY ] [USE INDEX or IGNORE INDEX]
    template<class fun_type>
    vector_record select(fun_type fun) {
        vector_record result;
        for (auto p : table) {
            if (fun(p)) {
                result.push_back(p);
            }
        }
        return result;
    }
    template<class fun_type>
    record find(fun_type fun) {
        for (auto p : table) { // linear
            if (fun(p)) {
                return p;
            }
        }
        return {};
    }
    template<class key_type>
    record find_with_index(key_type const &);
private:
    const usertable & ut() const { return table.ut(); }
    this_table & table;
    //FIXME: index_tree<cluster_index>
};

template<class this_table, class record> 
template<class key_type>
record make_query<this_table, record>::find_with_index(key_type const & key) {
    A_STATIC_ASSERT_TYPE(key_type, make_query::cluster_key);
    return {};
}

template<class META>
class base_cluster: public META { // is static
    using TYPE_LIST = typename META::type_list;
public:
    base_cluster() = delete;
    enum { index_size = TL::Length<TYPE_LIST>::value };        
    template<size_t i> using index_col = typename TL::TypeAt<TYPE_LIST, i>::Result;
protected:
    template<size_t i> static 
    const void * get_address(const void * const begin) {
        static_assert(i < index_size, "");
        return reinterpret_cast<const char *>(begin) + index_col<i>::offset;
    }
    template<size_t i> static 
    void * set_address(void * const begin) {
        static_assert(i < index_size, "");
        return reinterpret_cast<char *>(begin) + index_col<i>::offset;
    }
    template<size_t i> static 
    meta::index_type<TYPE_LIST, i> const & get_col(const void * const begin) {
        using T = meta::index_type<TYPE_LIST, i>;
        return * reinterpret_cast<T const *>(get_address<i>(begin));
    }
    template<size_t i> static 
    meta::index_type<TYPE_LIST, i> & set_col(void * const begin) {
        using T = meta::index_type<TYPE_LIST, i>;
        return * reinterpret_cast<T *>(set_address<i>(begin));
    }
};

namespace sample {
struct dbo_META {
    struct col {
        struct Id : meta::col<0, scalartype::t_int, 4, meta::key<true, 0, sortorder::ASC>> { static const char * name() { return "Id"; } };
        struct Id2 : meta::col<4, scalartype::t_bigint, 8, meta::key<true, 1, sortorder::ASC>> { static const char * name() { return "Id2"; } };
        struct Col1 : meta::col<12, scalartype::t_char, 255> { static const char * name() { return "Col1"; } };
    };
    typedef TL::Seq<
        col::Id
        ,col::Id2
        ,col::Col1
    >::Type type_list;
    struct cluster_META {
        using T0 = meta::index_col<col::Id>;
        using T1 = meta::index_col<col::Id2, T0::offset + sizeof(T0::type)>;
        typedef TL::Seq<T0, T1>::Type type_list;
    };
    class cluster_index : public base_cluster<cluster_META> {
        using base = base_cluster<cluster_META>;
    public:
#pragma pack(push, 1)
        struct key_type {
            T0::type _0;
            T1::type _1;
            template<size_t i>
            auto get() const -> decltype(base::get_col<i>(nullptr)) {
                return base::get_col<i>(this);
            }
            template<size_t i>
            auto set() -> decltype(base::set_col<i>(nullptr)) {
                return base::set_col<i>(this);
            }
        };
#pragma pack(pop)
        friend key_type;
        static const char * name() { return ""; }
    };
    static const char * name() { return ""; }
    static const int32 id = 0;
};
class dbo_table : public dbo_META, public make_base_table<dbo_META> {
    using base_table = make_base_table<dbo_META>;
    using this_table = dbo_table;
public:
    class record : public base_record<this_table> {
        using base = base_record<this_table>;
        using access = base_access<this_table, record>;
        using query = make_query<this_table, record>;
        friend access;
        friend query;
        friend this_table;
        record(this_table const * p, row_head const * h): base(p, h) {}
        record() = default;
    public:
        auto Id() const -> col::Id::ret_type { return val<col::Id>(); }
        auto Col1() const -> col::Col1::ret_type { return val<col::Col1>(); }
    };
public:
    using iterator = record::access::iterator;
    using query_type = record::query;
    explicit dbo_table(database * p, shared_usertable const & s)
        : base_table(p, s), _record(this, p, s) {}
    iterator begin() { return _record.begin(); }
    iterator end() { return _record.end(); }
    query_type query{ this };
    query_type * operator ->() { return &query; }
private:
    record::access _record;
};
} // sample
} // make
} // db
} // sdl

#endif // __SDL_SYSTEM_MAKETABLE_H__