// spatial_type.inl
//
#pragma once
#ifndef __SDL_SYSTEM_SPATIAL_TYPE_INL__
#define __SDL_SYSTEM_SPATIAL_TYPE_INL__

namespace sdl { namespace db {

inline bool operator == (spatial_point const & x, spatial_point const & y) { 
    return x.equal(y); 
}
inline bool operator != (spatial_point const & x, spatial_point const & y) { 
    return !(x == y);
}
inline bool operator < (spatial_point const & x, spatial_point const & y) { 
    if (x.longitude < y.longitude) return true;
    if (y.longitude < x.longitude) return false;
    return x.latitude < y.latitude;
}
inline spatial_point operator - (spatial_point const & p1, spatial_point const & p2) {
    return spatial_point::init(
        Latitude(p1.latitude - p2.latitude),
        Longitude(p1.longitude - p2.longitude));
}
//------------------------------------------------------------------------------------
inline bool operator < (spatial_cell const & x, spatial_cell const & y) {
    return spatial_cell::compare(x, y) < 0;
}
inline bool operator == (spatial_cell const & x, spatial_cell const & y) {
    return spatial_cell::equal(x, y);
}
inline bool operator != (spatial_cell const & x, spatial_cell const & y) {
    return !(x == y);
}
//------------------------------------------------------------------------------------
template<typename T>
inline bool operator == (point_XY<T> const & p1, point_XY<T> const & p2) {
    static_assert(std::is_floating_point<T>::value, "");
    return fequal(p1.X, p2.X) && fequal(p1.Y, p2.Y);
}
template<typename T>
inline bool operator != (point_XY<T> const & p1, point_XY<T> const & p2) {
    return !(p1 == p2);
}
template<typename T>
inline bool operator == (point_XYZ<T> const & p1, point_XYZ<T> const & p2) {
    static_assert(std::is_floating_point<T>::value, "");
    return fequal(p1.X, p2.X) && fequal(p1.Y, p2.Y) && fequal(p1.Z, p2.Z);
}
template<typename T>
inline bool operator != (point_XYZ<T> const & p1, point_XYZ<T> const & p2) {
    return !(p1 == p2);
}
template<typename T>
inline bool operator < (point_XY<T> const & p1, point_XY<T> const & p2) { 
    if (p1.X < p2.X) return true;
    if (p2.X < p1.X) return false;
    return p1.Y < p2.Y;
}
template<typename T>
inline point_XY<T> operator + (point_XY<T> const & p1, point_XY<T> const & p2) {
    return { p1.X + p2.X, p1.Y + p2.Y };
}
template<typename T>
inline point_XYZ<T> operator + (point_XYZ<T> const & p1, point_XYZ<T> const & p2) {
    return { p1.X + p2.X, p1.Y + p2.Y, p1.Z + p2.Z };
}
template<typename T>
inline point_XY<T> operator - (point_XY<T> const & p1, point_XY<T> const & p2) {
    return { p1.X - p2.X, p1.Y - p2.Y };
}
template<typename T>
inline point_XYZ<T> operator - (point_XYZ<T> const & p1, point_XYZ<T> const & p2) {
    return { p1.X - p2.X, p1.Y - p2.Y, p1.Z - p2.Z };
}
//------------------------------------------------------------------------------------
inline Degree degree(Radian const & x) {
    return limits::RAD_TO_DEG * x.value();
}
inline Radian radian(Degree const & x) {
    return limits::DEG_TO_RAD * x.value();
}
inline polar_2D polar(point_2D const & p) {
    return polar_2D::polar(p);
}
//------------------------------------------------------------------------------------
inline int spatial_cell::compare(spatial_cell const & x, spatial_cell const & y) {
    SDL_ASSERT(x.data.depth <= size);
    SDL_ASSERT(y.data.depth <= size);
    A_STATIC_ASSERT_TYPE(uint8, id_type);
    uint8 count = a_min(x.data.depth, y.data.depth);
    const uint8 * p1 = x.data.id;
    const uint8 * p2 = y.data.id;
    int v;
    while (count--) {
        if ((v = static_cast<int>(*(p1++)) - static_cast<int>(*(p2++))) != 0) {
            return v;
        }
    }
    return static_cast<int>(x.data.depth) - static_cast<int>(y.data.depth);
}
inline bool spatial_cell::equal(spatial_cell const & x, spatial_cell const & y) {
    SDL_ASSERT(x.data.depth <= size);
    SDL_ASSERT(y.data.depth <= size);
    A_STATIC_ASSERT_TYPE(uint8, id_type);
    if (x.data.depth != y.data.depth)
        return false;
    uint8 count = x.data.depth;
    const uint8 * p1 = x.data.id;
    const uint8 * p2 = y.data.id;
    while (count--) {
        if (*(p1++) != *(p2++)) {
            return false;
        }
    }
    return true;
}
//------------------------------------------------------------------------------------
inline bool spatial_rect::is_null() const {
    SDL_ASSERT(is_valid());
    return fequal(min_lon, max_lon) || fless_eq(max_lat, min_lat);
}
inline bool spatial_rect::cross_equator() const {
    SDL_ASSERT(is_valid());
    return (min_lat < 0) && (0 < max_lat);
}
inline spatial_point spatial_rect::min() const {
    return spatial_point::init(Latitude(min_lat), Longitude(min_lon));
}
inline spatial_point spatial_rect::max() const {
    return spatial_point::init(Latitude(max_lat), Longitude(max_lon));
}
inline spatial_rect spatial_rect::init(spatial_point const & p1, spatial_point const & p2) {
    spatial_rect rc;
    rc.min_lat = p1.latitude;
    rc.min_lon = p1.longitude;
    rc.max_lat = p2.latitude;
    rc.max_lon = p2.longitude;
    SDL_ASSERT(rc.is_valid());
    return rc;
}
inline spatial_rect spatial_rect::init(
                            Latitude min_lat, 
                            Longitude min_lon, 
                            Latitude max_lat, 
                            Longitude max_lon) {
    spatial_rect rc;
    rc.min_lat = min_lat.value();
    rc.min_lon = min_lon.value();
    rc.max_lat = max_lat.value();
    rc.max_lon = max_lon.value();
    SDL_ASSERT(rc.is_valid());
    return rc;
}
//------------------------------------------------------------------------------------
} // db
} // sdl

#endif // __SDL_SYSTEM_SPATIAL_TYPE_INL__