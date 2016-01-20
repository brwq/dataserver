// sysallocunits.h
//
#ifndef __SDL_SYSTEM_SYSALLOCUNITS_H__
#define __SDL_SYSTEM_SYSALLOCUNITS_H__

#pragma once

#include "page_head.h"

namespace sdl { namespace db {

#pragma pack(push, 1) 

struct sysallocunits_row_meta;
struct sysallocunits_row_info;

// System Table: sysallocunits (ObjectID = 7)
// The sysallocunits table is the entry point containing the metadata that describes all other tables in the database.
// The first page of this table is pointed to by the dbi_firstSysIndexes field on the boot page.
// The records in this table have 12 fixed length columns, a NULL bitmap, and a number of columns field.
// Most of the columns in this table can be seen through the DMVs sys.system_internals_allocation_units, 
// sys.partitions, sys.dm_db_partition_stats, and sys.allocation_units.
// The names in parenthesis are the columns as they appear in the DMV.

struct sysallocunits_row
{
    using meta = sysallocunits_row_meta;
    using info = sysallocunits_row_info;

    // The sysallocunits table is the entry point containing the metadata that describes all other tables in the database. 
    // The first page of this table is pointed to by the dbi_firstSysIndexes field on the boot page. 
    // The records in this table have 12 fixed length columns, a NULL bitmap, and a number of columns field.
    struct data_type {

        row_head        head;           // 4 bytes
        auid_t          auid;           // auid(allocation_unit_id / partition_id) - 8 bytes - the unique ID / primary key for this allocation unit.
        dataType        type;           // type(type) - 1 byte - 1 = IN_ROW_DATA, 2 = LOB_DATA, 3 = ROW_OVERFLOW_DATA
        auid_t          ownerid;        // ownerid(container_id / hobt_id) - 8 bytes - this is usually also an auid value, but sometimes not.
        uint32          status;         // status - 4 bytes - this column is not shown directly in any of the DMVs.
        uint16          fgid;           // fgid(filegroup_id) - 2 bytes
        pageFileID      pgfirst;        // pgfirst(first_page) - 6 bytes - page locator of the first data page for this allocation unit.
                                        // If this allocation unit has no data, then this will be 0:0, as will pgroot and pgfirstiam.
        pageFileID      pgroot;         // pgroot(root_page) - 6 bytes - page locator of the root page of the index tree, if this allocation units is for a B - tree.
        pageFileID      pgfirstiam;     // pgfirstiam(first_iam_page) - 6 bytes - page locator of the first IAM page for this allocation unit.
        uint64          pcused;         // pcused(used_pages) - 8 bytes - used page count - this is the number of data pages plus IAM and index pages(PageType IN(2, 10))
        uint64          pcdata;         // pcdata(data_pages) - 8 bytes - data page count - the numbr of pages specifically for data only(PageType IN(1, 3, 4))
        uint64          pcreserved;     // pcreserved(total_pages) - 8 bytes - reserved page count - this is the total number of pages used plus pages not yet used but reserved for future use.Reserving pages ahead of time is probably done to reduce locking time when more space needs to be allocated.
        uint32          dbfragid;       // dbfragid - 4 bytes - this column is not shown in the DMV
    };
    union {
        data_type data;
        char raw[sizeof(data_type)];
    };
};

// Every table/index has its own set of IAM pages, which are combined into separate linked lists called IAM chains.
// Each IAM chain covers its own allocation unit�IN_ROW_DATA, ROW_OVERFLOW_DATA, and LOB_DATA.

struct iam_page_row_meta;
struct iam_page_row_info;

struct iam_page_row
{
    using meta = iam_page_row_meta;
    using info = iam_page_row_info;

    struct data_type {

        row_head    head;           // 4 bytes
        uint32      seq;            // 00-03	SequenceNumber (int)
        uint8       _0x04[10];      // 04-13	?
        uint16      status;         // 14-15	Status (smallint)
        uint8       _0x10[12];		// 16-27	?
        int32       objectID;       // 28-31	ObjectID (int)
        int16       indexID;        // 32-33	IndexID (smallint)
        uint8       pageCount;      // 34		PageCount (tinyint)
        uint8       _0x23;          // 35		?
        pageFileID  startPage;      // 36-39	StartPage PageID (int)
		                            // 40-41	StartPage FileID (smallint)
        pageFileID  slot[8];        // 42-45	Slot0 PageID (int)
                                    // 46-47	Slot0 FileID (smallint) 
                                    // ...
                                    // 84-87	Slot7 PageID (int)
				                    // 88-89	Slot7 FileID (smallint)            
    };
    union {
        data_type data;
        char raw[sizeof(data_type)];
    };
};

#pragma pack(pop)

struct sysallocunits_row_meta: is_static {

    typedef_col_type_n(sysallocunits_row, head);
    typedef_col_type_n(sysallocunits_row, auid);
    typedef_col_type_n(sysallocunits_row, type);
    typedef_col_type_n(sysallocunits_row, ownerid);
    typedef_col_type_n(sysallocunits_row, status);
    typedef_col_type_n(sysallocunits_row, fgid);
    typedef_col_type_n(sysallocunits_row, pgfirst);
    typedef_col_type_n(sysallocunits_row, pgroot);
    typedef_col_type_n(sysallocunits_row, pgfirstiam);
    typedef_col_type_n(sysallocunits_row, pcused);
    typedef_col_type_n(sysallocunits_row, pcdata);
    typedef_col_type_n(sysallocunits_row, pcreserved);
    typedef_col_type_n(sysallocunits_row, dbfragid);

    typedef TL::Seq<
        head
        ,auid
        ,type
        ,ownerid
        ,status
        ,fgid
        ,pgfirst
        ,pgroot
        ,pgfirstiam
        ,pcused
        ,pcdata
        ,pcreserved
        ,dbfragid
    >::Type type_list;
};

struct sysallocunits_row_info: is_static {
    static std::string type_meta(sysallocunits_row const &);
    static std::string type_raw(sysallocunits_row const &);
};

struct iam_page_row_meta: is_static {

    typedef_col_type_n(iam_page_row, head);
    typedef_col_type_n(iam_page_row, seq);
    typedef_col_type_n(iam_page_row, status);
    typedef_col_type_n(iam_page_row, objectID);
    typedef_col_type_n(iam_page_row, indexID);
    typedef_col_type_n(iam_page_row, pageCount);
    typedef_col_type_n(iam_page_row, startPage);
    typedef_col_type_n(iam_page_row, slot);

    typedef TL::Seq<
        head
        ,seq
        ,status
        ,objectID
        ,indexID
        ,pageCount
        ,startPage
        ,slot
    >::Type type_list;
};

struct iam_page_row_info: is_static {
    static std::string type_meta(iam_page_row const &);
    static std::string type_raw(iam_page_row const &);
};

} // db
} // sdl

#endif // __SDL_SYSTEM_SYSALLOCUNITS_H__