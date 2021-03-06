// sysscalartypes.h
//
#pragma once
#ifndef __SDL_SYSOBJ_SYSSCALARTYPES_H__
#define __SDL_SYSOBJ_SYSSCALARTYPES_H__

#include "dataserver/system/page_head.h"

namespace sdl { namespace db {

#pragma pack(push, 1) 

struct sysscalartypes_row_meta;
struct sysscalartypes_row_info;

/* (ObjectID = 50)
The sysscalartypes table holds a row for every built-in and user-defined data type. 
It can be read using the DMVs sys.systypes and sys.types. */

struct sysscalartypes_row
{
    using meta = sysscalartypes_row_meta;
    using info = sysscalartypes_row_info;

    struct data_type { // 49 bytes

        row_head head; // 4 bytes

        scalartype      id;             // id - 4 bytes - the unique id for this built-in type or UDT.
        uint32          schid;          // schid - 4 bytes - the schema that owns this data type.
        column_xtype    xtype;          // xtype - 1 byte - the same as the xtype values in the syscolpars table - equal to the id for built-in types.
        scalarlen       length;         // length - 2 bytes - the length of the data type in bytes
        uint8           prec;           // prec - 1 byte
        uint8           scale;          // scale - 1 byte
        uint32          collationid;    // collationid - 4 bytes
        uint32          status;         // status - 4 bytes - status flags about the type.  If 0x1 is set, the type does not allow NULLs (default is to allow NULLs) 
        datetime_t      created;        // created - datetime, 8 bytes
        datetime_t      modified;       // modified - datetime, 8 bytes
        uint32          dflt;           // dflt - 4 bytes
        uint32          chk;            // chk - 4 bytes
    };
    union {
        data_type data;
        char raw[sizeof(data_type)];
    };
};

#pragma pack(pop)

template<> struct null_bitmap_traits<sysscalartypes_row> {
    enum { value = 1 };
};

template<> struct variable_array_traits<sysscalartypes_row> {
    enum { value = 1 };
};

struct sysscalartypes_row_meta: is_static {

    typedef_col_type_n(sysscalartypes_row, head);
    typedef_col_type_n(sysscalartypes_row, id);
    typedef_col_type_n(sysscalartypes_row, schid);
    typedef_col_type_n(sysscalartypes_row, xtype);
    typedef_col_type_n(sysscalartypes_row, length);
    typedef_col_type_n(sysscalartypes_row, prec);
    typedef_col_type_n(sysscalartypes_row, scale);
    typedef_col_type_n(sysscalartypes_row, collationid);
    typedef_col_type_n(sysscalartypes_row, status);
    typedef_col_type_n(sysscalartypes_row, created);
    typedef_col_type_n(sysscalartypes_row, modified);
    typedef_col_type_n(sysscalartypes_row, dflt);
    typedef_col_type_n(sysscalartypes_row, chk);

    typedef_var_col(0, nchar_range, name);

    typedef TL::Seq<
        head
        ,id
        ,schid
        ,xtype
        ,length
        ,prec
        ,scale
        ,collationid
        ,status
        ,created
        ,modified
        ,dflt
        ,chk
        ,name
    >::Type type_list;
};

struct sysscalartypes_row_info: is_static {
    static std::string type_meta(sysscalartypes_row const &);
    static std::string type_raw(sysscalartypes_row const &);
    static std::string col_name(sysscalartypes_row const &);
};

} // db
} // sdl

#endif // __SDL_SYSOBJ_SYSSCALARTYPES_H__

