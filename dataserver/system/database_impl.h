// database_impl.h
//
#pragma once
#ifndef __SDL_SYSTEM_DATABASE_IMPL_H__
#define __SDL_SYSTEM_DATABASE_IMPL_H__

#include "common/map_enum.h"
#include <map>
#include <algorithm>
#include <mutex>

#define SDL_DATABASE_LOCK_ENABLED       0

namespace sdl { namespace db {

class database_PageMapping {
public:
    database_PageMapping(const database_PageMapping&) = delete;
    database_PageMapping& operator=(const database_PageMapping&) = delete;
    const PageMapping pm;
protected:
    explicit database_PageMapping(const std::string & fname): pm(fname){}
    ~database_PageMapping(){}
};

class database::shared_data final : public database_PageMapping {
    using map_sysalloc = compact_map<schobj_id, shared_sysallocunits>;
    using map_datapage = compact_map<schobj_id, shared_page_head_access>;
    using map_index = compact_map<schobj_id, pgroot_pgfirst>;
    using map_primary = compact_map<schobj_id, shared_primary_key>;
    using map_cluster = compact_map<schobj_id, shared_cluster_index>;
    using map_spatial_tree = compact_map<schobj_id, spatial_tree_idx>;
    struct data_type {
        shared_usertables usertable;
        shared_usertables internal;
        shared_datatables datatable;
        map_enum_1<map_sysalloc, dataType> sysalloc;
        map_enum_2<map_datapage, dataType, pageType> datapage; // not preloaded in init_database()
        map_enum_1<map_index, pageType> index;
        map_primary primary;
        map_cluster cluster;
        map_spatial_tree spatial_tree;
        data_type()
            : usertable(std::make_shared<vector_shared_usertable>())
            , internal(std::make_shared<vector_shared_usertable>())
            , datatable(std::make_shared<vector_shared_datatable>())
        {}
    };
public:
    bool initialized = false;
    explicit shared_data(const std::string & fname): database_PageMapping(fname){}
#if SDL_DATABASE_LOCK_ENABLED    
    shared_usertables & usertable() {
        return m_data.usertable;
    } 
    shared_usertables & internal() {
        return m_data.usertable;
    }
    shared_datatables & datatable() {
        return m_data.datatable;
    }
    bool empty_usertable() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_data.usertable->empty();
    }
    bool empty_internal() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_data.internal->empty();
    }
    bool empty_datatable() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_data.datatable->empty();
    }
    shared_sysallocunits find_sysalloc(schobj_id const id, dataType::type const data_type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto found = m_data.sysalloc.find(id, data_type)) {
            return *found;
        }
        return{};
    }
    void set_sysalloc(schobj_id const id, dataType::type const data_type,
                      shared_sysallocunits const & value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.sysalloc(id, data_type) = value;
    }
    shared_page_head_access find_datapage(schobj_id const id, 
                                          dataType::type const data_type,
                                          pageType::type const page_type)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto found = m_data.datapage.find(id, data_type, page_type)) {
            return *found;
        }
        return{};
    }
    void set_datapage(schobj_id const id, 
                      dataType::type const data_type,
                      pageType::type const page_type,
                      shared_page_head_access const & value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.datapage(id, data_type, page_type) = value;
    }
    pgroot_pgfirst load_pg_index(schobj_id const id, pageType::type const page_type)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto found = m_data.index.find(id, page_type)) {
            return *found;
        }
        return{};
    }
    void set_pg_index(schobj_id const id, pageType::type const page_type, pgroot_pgfirst const & value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.index(id, page_type) = value;
    }
    shared_primary_key get_primary_key(schobj_id const table_id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto const found = m_data.primary.find(table_id);
        if (found != m_data.primary.end()) {
            return found->second;
        }
        return{};
    }
    void set_primary_key(schobj_id const table_id, shared_primary_key const & value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.primary[table_id] = value;
    }
private:
    std::mutex m_mutex;
#else
    data_type const & const_data() const { return m_data; }
    data_type & data() { return m_data; }
#endif
private:
    data_type m_data;
};

} // db
} // sdl

#endif // __SDL_SYSTEM_DATABASE_IMPL_H__