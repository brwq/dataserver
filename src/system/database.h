// database.h
//
#ifndef __SDL_SYSTEM_DATABASE_H__
#define __SDL_SYSTEM_DATABASE_H__

#pragma once

#include "datapage.h"
#include "usertable.h"
#include "page_iterator.h"

namespace sdl { namespace db {

class datatable;

class database: noncopyable
{
    enum class sysObj {
        sysallocunits = 7,
        sysschobjs = 34,
        syscolpars = 41,
        sysscalartypes = 50,
        sysidxstats = 54,
        sysiscols = 55,
        sysobjvalues = 60,
    };
    enum class sysPage {
        file_header = 0,
        boot_page = 9,
    };
public:
    template<class T> using page_ptr = std::shared_ptr<T>;

    using shared_usertable = std::shared_ptr<usertable>;
    using vector_shared_usertable = std::vector<shared_usertable>;
    
    using shared_datatable = std::shared_ptr<datatable>; 
    using vector_shared_datatable = std::vector<shared_datatable>; 

    using unique_datatable = std::unique_ptr<datatable>;
    using shared_datapage = std::shared_ptr<datapage>; 
    using shared_iam_page = std::shared_ptr<iam_page>;
public:   
    void load_page(page_ptr<sysallocunits> &);
    void load_page(page_ptr<sysschobjs> &);
    void load_page(page_ptr<syscolpars> &);
    void load_page(page_ptr<sysidxstats> &);
    void load_page(page_ptr<sysscalartypes> &);
    void load_page(page_ptr<sysobjvalues> &);
    void load_page(page_ptr<sysiscols> &);

    void load_next(page_ptr<sysallocunits> &);
    void load_next(page_ptr<sysschobjs> &);
    void load_next(page_ptr<syscolpars> &);
    void load_next(page_ptr<sysidxstats> &);
    void load_next(page_ptr<sysscalartypes> &);
    void load_next(page_ptr<sysobjvalues> &);
    void load_next(page_ptr<sysiscols> &);

    void load_prev(page_ptr<sysallocunits> &);
    void load_prev(page_ptr<sysschobjs> &);
    void load_prev(page_ptr<syscolpars> &);
    void load_prev(page_ptr<sysidxstats> &);
    void load_prev(page_ptr<sysscalartypes> &);
    void load_prev(page_ptr<sysobjvalues> &);
    void load_prev(page_ptr<sysiscols> &);

    void load_next(shared_datapage &);
    void load_prev(shared_datapage &);

    void load_next(shared_iam_page &);
    void load_prev(shared_iam_page &);

    template<typename T> void load_page(T&) = delete;
    template<typename T> void load_next(T&) = delete;
    template<typename T> void load_prev(T&) = delete;

    template<typename T> static
    bool is_same(T const & p1, T const & p2) {
        if (p1 && p2) {
            A_STATIC_CHECK_TYPE(page_head const * const, p1->head);
            return p1->head == p2->head;
        }
        return p1 == p2; // both nullptr
    }
private: 
    template<class pointer_type>
    class page_access_t : noncopyable {
        database * const db;
    public:
        using iterator = page_iterator<database, pointer_type>;
        iterator begin() {
            pointer_type p{};
            db->load_page(p);
            return iterator(db, std::move(p));
        }
        iterator end() {
            return iterator(db);
        }
        explicit page_access_t(database * p): db(p) {
            SDL_ASSERT(db);
        }
    };
    template<class T>
    using page_access = page_access_t<page_ptr<T>>;    

    class usertable_access : noncopyable {
        database * const db;
        vector_shared_usertable const & data() {
            return db->get_usertables();
        }
    public:
        using iterator = vector_shared_usertable::const_iterator;
        iterator begin() {
            return data().begin();
        }
        iterator end() {
            return data().end();
        }
        explicit usertable_access(database * p): db(p) {
            SDL_ASSERT(db);
        }
    };
    class datatable_access : noncopyable {
        database * const db;
        vector_shared_datatable const & data() {
            return db->get_datatable();
        }
    public:
        using iterator = vector_shared_datatable::const_iterator;
        iterator begin() {
            return data().begin();
        }
        iterator end() {
            return data().end();
        }
        explicit datatable_access(database * p): db(p) {
            SDL_ASSERT(db);
        }
    };
private:
    page_head const * load_sys_obj(sysallocunits const *, sysObj);

    template<class T, sysObj id> 
    page_ptr<T> get_sys_obj(sysallocunits const *);
    
    template<class T, sysObj id> 
    page_ptr<T> get_sys_obj();

    template<class T>
    void load_next_t(page_ptr<T> &);

    template<class T>
    void load_prev_t(page_ptr<T> &);

    template<class T, class fun_type> static
    typename T::const_pointer 
    find_row_if(page_access<T> & obj, fun_type fun) {
        for (auto & p : obj) {
            if (auto found = p->find_if(fun)) {
                return found;
            }
        }
        return nullptr;
    }
    template<class T, class fun_type> static
    void for_row(page_access<T> & obj, fun_type fun) {
        for (auto & p : obj) {
            p->for_row(fun);
        }
    }
    template<class fun_type>
    unique_datatable find_table_if(fun_type);
public:
    explicit database(const std::string & fname);
    ~database();

    const std::string & filename() const;

    bool is_open() const;

    size_t page_count() const;

    page_head const * load_page_head(pageIndex);
    page_head const * load_page_head(pageFileID const &);

    page_head const * load_next_head(page_head const *);
    page_head const * load_prev_head(page_head const *);
    
    void const * start_address() const; // diagnostic only
    void const * memory_offset(void const *) const; // diagnostic only

    pageType get_pageType(pageFileID const &); // diagnostic

    page_ptr<bootpage> get_bootpage();
    page_ptr<fileheader> get_fileheader();
    page_ptr<datapage> get_datapage(pageIndex);

    page_ptr<sysallocunits> get_sysallocunits();
    page_ptr<sysschobjs> get_sysschobjs();
    page_ptr<syscolpars> get_syscolpars();
    page_ptr<sysidxstats> get_sysidxstats();
    page_ptr<sysscalartypes> get_sysscalartypes();
    page_ptr<sysobjvalues> get_sysobjvalues();
    page_ptr<sysiscols> get_sysiscols();   

    page_access<sysallocunits> _sysallocunits{this};
    page_access<sysschobjs> _sysschobjs{this};
    page_access<syscolpars> _syscolpars{this};
    page_access<sysidxstats> _sysidxstats{this};
    page_access<sysscalartypes> _sysscalartypes{this};
    page_access<sysobjvalues> _sysobjvalues{this};
    page_access<sysiscols> _sysiscols{this};

    usertable_access _usertables{this};
    datatable_access _datatables{this};

    unique_datatable find_table_name(const std::string & name);

    using datapage_iterator = page_iterator<database, shared_datapage>;
    datapage_iterator begin_datapage(schobj_id, pageType::type);
    datapage_iterator end_datapage() {
        return datapage_iterator(this);
    }

    using iam_page_iterator = page_iterator<database, shared_iam_page>;
    iam_page_iterator begin_iam_page(schobj_id);
    iam_page_iterator end_iam_page() {
        return iam_page_iterator(this);
    }
    using vector_sysallocunits_row = std::vector<sysallocunits_row const *>;
    vector_sysallocunits_row find_sysalloc(schobj_id); 
private:
    page_head const * load_page_head(schobj_id, pageType::type);

    template<class fun_type>
    void for_sysschobjs(fun_type fun) {
        for (auto & p : _sysschobjs) {
            p->for_row(fun);
        }
    }
    template<class fun_type>
    void for_USER_TABLE(fun_type fun) {
        for_sysschobjs([&fun](sysschobjs::const_pointer row){
            if (row->is_USER_TABLE_id()) {
                fun(row);
            }
        });
    }
    vector_shared_usertable const & get_usertables();
    vector_shared_datatable const & get_datatable();
private:
    page_head const * load_page_head(sysPage);
    std::vector<page_head const *> load_page_list(page_head const *);
private:
    class data_t;
    //class PageMapping;
    std::unique_ptr<data_t> m_data;
};

class datatable : noncopyable
{
    using shared_usertable = database::shared_usertable;
    using shared_datapage = database::shared_datapage;

    database * const db;
    shared_usertable const schema;
private:
    class datapage_access : noncopyable {
        datatable * const table;
        pageType::type const type;
    public:
        using iterator = database::datapage_iterator;
        explicit datapage_access(datatable * p, pageType::type t)
            : table(p), type(t)
        {
            SDL_ASSERT(table);
        }
        iterator begin() {
            return table->db->begin_datapage(table->ut().get_id(), type);
        }
        iterator end() {
            return table->db->end_datapage();
        }
    };
    class iam_page_access : noncopyable {
        datatable * const table;
    public:
        using iterator = database::iam_page_iterator;
        explicit iam_page_access(datatable * p) : table(p) {
            SDL_ASSERT(table);
        }
        iterator begin() {
            return table->db->begin_iam_page(table->ut().get_id());
        }
        iterator end() {
            return table->db->end_iam_page();
        }
    };
    class sysalloc_access : noncopyable {
        using data_type = database::vector_sysallocunits_row;        
        datatable * const table;
        std::pair<data_type, bool> data;
        data_type const & sysalloc() {
            if (!data.second) {
                data.second = true;
                data.first = table->db->find_sysalloc(table->ut().get_id());
            }
            return data.first;
        }
    public:
        using iterator = data_type::const_iterator;
        explicit sysalloc_access(datatable * p) : table(p) {
            SDL_ASSERT(table);
        }
        iterator begin() {
            return sysalloc().begin();
        }
        iterator end() {
            return sysalloc().end();
        }
    };
public:
    datatable(database * p, shared_usertable const & t)
        : db(p), schema(t)
    {
        SDL_ASSERT(db && schema);
    }
    ~datatable(){}

    const usertable & ut() const {
        return *schema.get();
    }

    datapage_access _datapages{ this, pageType::type::data };
    iam_page_access _iampages{ this };
    sysalloc_access _sysalloc{ this };

    //TODO: parse iam page
    //TODO: row iterator -> column[] -> column type, name, length, value 
};

#if 0
SQL Server tracks the pages and extents used by the different types of pages (in-row, row-overflow, and LOB pages), 
that belong to the object with another set of the allocation map pages, called Index Allocation Map (IAM).
Every table/index has its own set of IAM pages, which are combined into separate linked lists called IAM chains.
Each IAM chain covers its own allocation unit�IN_ROW_DATA, ROW_OVERFLOW_DATA, and LOB_DATA.
Each IAM page in the chain covers a particular GAM interval and represents the bitmap where each bit indicates
if a corresponding extent stores the data that belongs to a particular allocation unit for a particular object. 
In addition, the first IAM page for the object stores the actual page addresses for the first eight object pages, 
which are stored in mixed extents.
#endif

} // db
} // sdl

#endif // __SDL_SYSTEM_DATABASE_H__
