// spatial_type.h
//
#pragma once
#ifndef __SDL_SYSTEM_SPATIAL_TYPE_H__
#define __SDL_SYSTEM_SPATIAL_TYPE_H__

#include "page_type.h"

namespace sdl { namespace db {

namespace unit {
    struct Latitude{};
    struct Longitude{};
    struct Meters{};
    struct Kilometers{};
}
typedef quantity<unit::Latitude, double> Latitude;      // in degrees -90..90
typedef quantity<unit::Longitude, double> Longitude;    // in degrees -180..180
typedef quantity<unit::Meters, double> Meters;
typedef quantity<unit::Kilometers, double> Kilometers;

enum class spatial_type {
    null = 0,
    point = 0x0C01,
    multipolygon = 0x0401,
    linestring = 0x1401
};

//template<spatial_type T> using spatial_t = Val2Type<spatial_type, T>;

#pragma pack(push, 1) 

struct spatial_cell { // 5 bytes
    
    using id_type = uint8;
    static const size_t size = 4; // max depth

    struct data_type { // 5 bytes
        id_type id[size];
        id_type depth;   // [1..4]
    };
    union {
        data_type data;
        char raw[sizeof(data_type)];
    };
    id_type operator[](size_t i) const {
        SDL_ASSERT(i < size);
        return data.id[i];
    }
    id_type & operator[](size_t i) {
        SDL_ASSERT(i < size);
        return data.id[i];
    }
    bool is_null() const {
        SDL_ASSERT(data.depth <= size);
        return 0 == data.depth;
    }
    explicit operator bool() const {
        return !is_null();
    }
    size_t depth() const {
        SDL_ASSERT(data.depth <= size);
        return data.depth; // (data.depth <= size) ? size_t(data.depth) : size;
    }
    void set_depth(size_t const d) {
        SDL_ASSERT(d && (d <= 4));
        data.depth = (id_type)a_min<size_t>(d, 4);
    }
    static spatial_cell min();
    static spatial_cell max();
    static spatial_cell parse_hex(const char *);
    bool intersect(spatial_cell const &) const;
    static int compare(spatial_cell const &, spatial_cell const &);
    static bool equal(spatial_cell const &, spatial_cell const &);
#if SDL_DEBUG
    static bool test_depth(spatial_cell const &);
#endif
};

struct spatial_point { // 16 bytes

    double latitude;
    double longitude;

    static constexpr double min_latitude    = -90;
    static constexpr double max_latitude    = 90;
    static constexpr double min_longitude   = -180;
    static constexpr double max_longitude   = 180;

    static bool is_valid(Latitude const d) {
        return fless_equal(d.value(), max_latitude) && fless_equal(min_latitude, d.value());
    }
    static bool is_valid(Longitude const d) {
        return fless_equal(d.value(), max_longitude) && fless_equal(min_longitude, d.value());
    }
    bool is_valid() const {
        return is_valid(Latitude(this->latitude)) && is_valid(Longitude(this->longitude));
    }
    static spatial_point init(Latitude const lat, Longitude const lon) {
        SDL_ASSERT(is_valid(lat) && is_valid(lon));
        return { lat.value(), lon.value() };
    }
    bool equal(spatial_point const & y) const {
        return fequal(latitude, y.latitude) && fequal(longitude, y.longitude); 
    }
    static spatial_point STPointFromText(const std::string &); // POINT (longitude latitude)
};

template<typename T>
struct point_XY {
    using type = T;
    type X, Y;
};

template<typename T>
struct point_XYZ {
    using type = T;
    type X, Y, Z;
};

struct spatial_grid { // 4 bytes
    static const size_t size = spatial_cell::size;
    enum grid_size : uint8 {
        LOW     = 4,    // 4X4,     16 cells
        MEDIUM  = 8,    // 8x8,     64 cells
        HIGH    = 16    // 16x16,   256 cells
    };
    grid_size level[size];
    spatial_grid(): level{HIGH, HIGH, HIGH, HIGH} {}
    explicit spatial_grid(
        grid_size const s0,
        grid_size const s1 = HIGH,
        grid_size const s2 = HIGH,
        grid_size const s3 = HIGH) {
        level[0] = s0; level[1] = s1;
        level[2] = s2; level[3] = s3;
        static_assert(size == 4, "");
        static_assert(HIGH * HIGH == 1 + spatial_cell::id_type(-1), "");
    }
    int operator[](size_t i) const {
        SDL_ASSERT(i < size);
        return level[i];
    }
};

#pragma pack(pop)

struct transform : is_static {
    using grid_size = spatial_grid::grid_size;
    static spatial_cell make_cell(spatial_point const &, spatial_grid const & = {});
    static point_XY<int> make_hil(spatial_cell::id_type, grid_size = grid_size::HIGH); // for diagnostics (hilbert::d2xy)
    static point_XY<double> make_pt(spatial_cell const &, spatial_grid const & = {}); // for diagnostics (point inside square 1x1)
};

} // db
} // sdl

#include "spatial_type.inl"

#endif // __SDL_SYSTEM_SPATIAL_TYPE_H__