// file_header.h
//
#ifndef __SDL_SYSTEM_FILE_HEADER_H__
#define __SDL_SYSTEM_FILE_HEADER_H__

#pragma once

#include "page_head.h"

namespace sdl { namespace db {

#pragma pack(push, 1) 

struct fileheader_field
{
    struct data_type 
    {
        uint16      _0x00;           // 0x00 : 2 bytes - value 0x0030
        uint16      _0x02;           // 0x02 : 2 bytes - value 0x0008
        uint32      _0x04;           // 0x04 : 4 bytes - value 0
        uint32      _0x08;           // 0x08 : 4 bytes - value 47 (49 ?)
        //uint32      _0x0C;           // 0x0C : 4 bytes - value 0 ?
        //uint16      NumberFields;    // 0x10 : NumberFields - 2 bytes - count of number fields
        //uint16      FieldEndOffsets; // 0x12 : FieldEndOffsets - 2 * (NumFields)-offset to the end of each field in bytes relative to the start of the page.
    };
    union {
        data_type data;
        char raw[sizeof(data_type)];
    };
};

// The first page in each database file is the file header page, 
// and there is only one such page per file.
struct fileheader_row
{
    //FIXME: to be tested
    struct data_type
    {
        record_head        head;   // 4 bytes
        fileheader_field   field;  // The fixed length file header fields, followed by the offsets for the variable length fields
    };
    union {
        enum { dump_raw = 0x438 };    
        data_type data;
        char raw[sizeof(data_type) > dump_raw ? sizeof(data_type) : dump_raw];
    };
};

#pragma pack(pop)

template<> struct row_traits<fileheader_row> {
    enum { null_bitmap = 1 };
};

struct fileheader_field_meta {

    typedef_col_type_n(fileheader_field, _0x00);
    typedef_col_type_n(fileheader_field, _0x02);
    typedef_col_type_n(fileheader_field, _0x04);
    typedef_col_type_n(fileheader_field, _0x08);
    //typedef_col_type_n(fileheader_field, _0x0C);
    //typedef_col_type_n(fileheader_field, NumberFields);
    //typedef_col_type_n(fileheader_field, FieldEndOffsets);

    typedef TL::Seq<
             _0x00
            ,_0x02
            ,_0x04
            ,_0x08
            //,_0x0C
            //,NumberFields
            //,FieldEndOffsets
    >::Type type_list;

    fileheader_field_meta() = delete;
};

struct fileheader_row_meta {

    typedef_col_type_n(fileheader_row, head);
    typedef_col_type_n(fileheader_row, field);

    typedef TL::Seq<
            head
            ,field
    >::Type type_list;

    fileheader_row_meta() = delete;
};

struct fileheader_row_info {
    fileheader_row_info() = delete;
    static std::string type_meta(fileheader_row const &);
    static std::string type_raw(fileheader_row const &);
};

} // db
} // sdl

#endif // __SDL_SYSTEM_FILE_HEADER_H__
