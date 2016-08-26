﻿// transform_math.cpp
//
#include "common/common.h"
#include "transform_math.h"
#include "hilbert.inl"
#include "transform.inl"
#include "math_util.h"
#include "system/page_info.h"
#include "common/static_type.h"
#include <algorithm>
#include <cstdlib>
#include <iomanip> // for std::setprecision

namespace sdl { namespace db { namespace space { 

#if SDL_DEBUG && defined(SDL_OS_WIN32)
namespace {
    template<class T>
    void debug_trace(T const & v) {
        size_t i = 0;
        for (auto & p : v) {
            std::cout << (i++)
                << std::setprecision(9)
                << "," << p.X
                << "," << p.Y
                << "\n";
        }
    }
}
#endif

const double math::sorted_quadrant[quadrant_size] = { -135, -45, 45, 135 };

const point_2D math::north_quadrant[quadrant_size] = {
    { 1, 0.5 },
    { 1, 1 },  
    { 0, 1 },  
    { 0, 0.5 } 
};
const point_2D math::south_quadrant[quadrant_size] = {
    { 1, 0.5 },
    { 1, 0 },
    { 0, 0 },
    { 0, 0.5 } 
};

const point_2D math::pole_hemisphere[2] = {
    { 0.5, 0.75 }, // north
    { 0.5, 0.25 }  // south
};

math::quadrant math::longitude_quadrant(double const x) {
    SDL_ASSERT(SP::valid_longitude(x));
    if (x >= 0) {
        if (x < 45) return q_0;
        if (x < 135) return q_1;
    }
    else {
        if (x >= -45) return q_0;
        if (x >= -135) return q_3;
    }
    return q_2; 
}

point_3D math::cartesian(Latitude const lat, Longitude const lon) { // 3D point on the globe
    SDL_ASSERT(spatial_point::is_valid(lat));
    SDL_ASSERT(spatial_point::is_valid(lon));
    double const L = std::cos(lat.value() * limits::DEG_TO_RAD);
    point_3D p;
    p.X = L * std::cos(lon.value() * limits::DEG_TO_RAD);
    p.Y = L * std::sin(lon.value() * limits::DEG_TO_RAD);
    p.Z = std::sin(lat.value() * limits::DEG_TO_RAD);
    return p;
}

spatial_point math::reverse_cartesian(point_3D const & p) { // p = 3D point on the globe
    SDL_ASSERT(fequal(length(p), 1));
    spatial_point s;
    if (p.Z >= 1.0 - limits::fepsilon)
        s.latitude = 90; 
    else if (p.Z <= -1.0 + limits::fepsilon)
        s.latitude = -90;
    else
        s.latitude = std::asin(p.Z) * limits::RAD_TO_DEG;
    s.longitude = fatan2(p.Y, p.X) * limits::RAD_TO_DEG;
    SDL_ASSERT(s.is_valid());
    return s;
}

namespace line_plane_intersect_ {
    static const point_3D P0 { 0, 0, 0 };
    static const point_3D V0 { 1, 0, 0 };
    static const point_3D e2 { 0, 1, 0 };
    static const point_3D e3 { 0, 0, 1 };
    static const point_3D N = normalize(point_3D{ 1, 1, 1 }); // plane P be given by a point V0 on it and a normal vector N
}

inline spatial_point math::reverse_line_plane_intersect(point_3D const & p)
{
    namespace use = line_plane_intersect_;
    SDL_ASSERT(frange(p.X, 0, 1));
    SDL_ASSERT(frange(p.Y, 0, 1));
    SDL_ASSERT(frange(p.Z, 0, 1));
    SDL_ASSERT(p != use::P0);
    return reverse_cartesian(normalize(p));
}

point_3D math::line_plane_intersect(Latitude const lat, Longitude const lon) { //http://geomalgorithms.com/a05-_intersect-1.html

    SDL_ASSERT(frange(lon.value(), 0, 90));
    SDL_ASSERT(frange(lat.value(), 0, 90));

    namespace use = line_plane_intersect_;
    const point_3D ray = cartesian(lat, lon); // cartesian position on globe
    const double n_u = scalar_mul(ray, use::N);
    
    SDL_ASSERT(fequal(length(ray), 1));
    SDL_ASSERT(n_u > 0);
    SDL_ASSERT(fequal(scalar_mul(use::N, use::N), 1.0));
    SDL_ASSERT(fequal(scalar_mul(use::N, use::V0), use::N.X)); // = N.X
    SDL_ASSERT(!point_on_plane(use::P0, use::V0, use::N));
    SDL_ASSERT(point_on_plane(use::e2, use::V0, use::N));
    SDL_ASSERT(point_on_plane(use::e3, use::V0, use::N));

    point_3D const p = multiply(ray, use::N.X / n_u); // distance = N * (V0 - P0) / n_u = N.X / n_u
    SDL_ASSERT(frange(p.X, 0, 1));
    SDL_ASSERT(frange(p.Y, 0, 1));
    SDL_ASSERT(frange(p.Z, 0, 1));
    SDL_ASSERT(p != use::P0);
    return p;
}

double math::longitude_meridian(double const x, const quadrant q) { // x = longitude
    SDL_ASSERT(a_abs(x) <= 180);
    if (x >= 0) {
        switch (q) {
        case q_0: return x + 45;
        case q_1: return x - 45;
        default:
            SDL_ASSERT(q == q_2);
            return x - 135;
        }
    }
    else {
        switch (q) {
        case q_0: return x + 45;
        case q_3: return x + 135;
        default:
            SDL_ASSERT(q == q_2);
            return x + 180 + 45;
        }
    }
}

double math::reverse_longitude_meridian(double const x, const quadrant q) { // x = longitude 
    SDL_ASSERT(frange(x, 0, 90));
    switch (q) {
    case q_0: return x - 45;
    case q_1: return x + 45;
    case q_2:
        if (x <= 45) {
            return x + 135;
        }
        return x - 180 - 45;
    default:
        SDL_ASSERT(q == q_3);
        return x - 135;
    }
}

namespace scale_plane_intersect_ {
    static const point_3D e1 { 1, 0, 0 };
    static const point_3D e2 { 0, 1, 0 };
    static const point_3D e3 { 0, 0, 1 };
    static const point_3D mid{ 0.5, 0.5, 0 };
    static const point_3D px = normalize(minus_point(e2, e1));
    static const point_3D py = normalize(minus_point(e3, mid));
    static const double lx = distance(e2, e1);
    static const double ly = distance(e3, mid);
    static const point_2D scale_02 { 0.5 / lx, 0.5 / ly };
    static const point_2D scale_13 { 1 / lx, 0.25 / ly };
}

point_2D math::scale_plane_intersect(const point_3D & p3, const quadrant quad, const hemisphere is_north)
{
    namespace use = scale_plane_intersect_;

    SDL_ASSERT_1(fequal(length(use::px), 1.0));
    SDL_ASSERT_1(fequal(length(use::py), 1.0));
    SDL_ASSERT_1(fequal(use::lx, std::sqrt(2.0)));
    SDL_ASSERT_1(fequal(use::ly, std::sqrt(1.5)));

    const point_3D v3 = minus_point(p3, use::e1);
    point_2D p2 = { 
        scalar_mul(v3, use::px), 
        scalar_mul(v3, use::py) };

    SDL_ASSERT_1(frange(p2.X, 0, use::lx));
    SDL_ASSERT_1(frange(p2.Y, 0, use::ly));

    if (quad & 1) { // 1, 3
        p2.X *= use::scale_13.X;
        p2.Y *= use::scale_13.Y;
        SDL_ASSERT_1(frange(p2.X, 0, 1));
        SDL_ASSERT_1(frange(p2.Y, 0, 0.25));
    }
    else { // 0, 2
        p2.X *= use::scale_02.X;
        p2.Y *= use::scale_02.Y;
        SDL_ASSERT_1(frange(p2.X, 0, 0.5));
        SDL_ASSERT_1(frange(p2.Y, 0, 0.5));
    }
    point_2D ret;
    if (hemisphere::north == is_north) {
        switch (quad) {
        case q_0:
            ret.X = 1 - p2.Y;
            ret.Y = 0.5 + p2.X;
            break;
        case q_1:
            ret.X = 1 - p2.X;
            ret.Y = 1 - p2.Y;
            break;
        case q_2:
            ret.X = p2.Y;
            ret.Y = 1 - p2.X;
            break;
        default:
            SDL_ASSERT(q_3 == quad);
            ret.X = p2.X;
            ret.Y = 0.5 + p2.Y;
            break;
        }
    }
    else {
        switch (quad) {
        case q_0:
            ret.X = 1 - p2.Y;
            ret.Y = 0.5 - p2.X;
            break;
        case q_1:
            ret.X = 1 - p2.X;
            ret.Y = p2.Y;
            break;
        case q_2:
            ret.X = p2.Y;
            ret.Y = p2.X;
            break;
        default:
            SDL_ASSERT(q_3 == quad);
            ret.X = p2.X;
            ret.Y = 0.5 - p2.Y;
            break;
        }
    }
    SDL_ASSERT_1(frange(ret.X, 0, 1));
    SDL_ASSERT_1(frange(ret.Y, 0, 1));
    return ret;
}

point_3D math::reverse_scale_plane_intersect(point_2D const & ret, quadrant const quad, const hemisphere is_north) 
{
    namespace use = scale_plane_intersect_;

    SDL_ASSERT_1(frange(ret.X, 0, 1));
    SDL_ASSERT_1(frange(ret.Y, 0, 1));

    //1) reverse scaling quadrant
    point_2D p2;
    if (hemisphere::north == is_north) {
        switch (quad) {
        case q_0:
            p2.Y = 1 - ret.X;   // ret.X = 1 - p2.Y;
            p2.X = ret.Y - 0.5; // ret.Y = 0.5 + p2.X;
            break;
        case q_1:
            p2.X = 1 - ret.X;   // ret.X = 1 - p2.X;
            p2.Y = 1 - ret.Y;   // ret.Y = 1 - p2.Y;
            break;
        case q_2:
            p2.Y = ret.X;       // ret.X = p2.Y;
            p2.X = 1 - ret.Y;   // ret.Y = 1 - p2.X;
            break;
        default:
            SDL_ASSERT(q_3 == quad);
            p2.X = ret.X;       // ret.X = p2.X;
            p2.Y = ret.Y - 0.5; // ret.Y = 0.5 + p2.Y;
            break;
        }
    }
    else {
        switch (quad) {
        case q_0:
            p2.Y = 1 - ret.X;   // ret.X = 1 - p2.Y;
            p2.X = 0.5 - ret.Y; // ret.Y = 0.5 - p2.X;
            break;
        case q_1:
            p2.X = 1 - ret.X;   // ret.X = 1 - p2.X;
            p2.Y = ret.Y;       // ret.Y = p2.Y;
            break;
        case q_2:
            p2.Y = ret.X;       // ret.X = p2.Y;
            p2.X = ret.Y;       // ret.Y = p2.X;
            break;
        default:
            SDL_ASSERT(q_3 == quad);
            p2.X = ret.X;       // ret.X = p2.X;
            p2.Y = 0.5 - ret.Y; // ret.Y = 0.5 - p2.Y;
            break;
        }
    }
    if (quad & 1) { // 1, 3
        SDL_ASSERT_1(frange(p2.X, 0, 1));
        SDL_ASSERT_1(frange(p2.Y, 0, 0.25));
        p2.X /= use::scale_13.X;
        p2.Y /= use::scale_13.Y;
    }
    else { // 0, 2
        SDL_ASSERT_1(frange(p2.X, 0, 0.5));
        SDL_ASSERT_1(frange(p2.Y, 0, 0.5));
        p2.X /= use::scale_02.X;
        p2.Y /= use::scale_02.Y;
    }
    //2) re-project 2D to 3D 
    SDL_ASSERT_1(frange(p2.X, 0, use::lx));
    SDL_ASSERT_1(frange(p2.Y, 0, use::ly));
    return use::e1 + multiply(use::px, p2.X) + multiply(use::py, p2.Y);
}

point_2D math::project_globe(spatial_point const & s, hemisphere const h)
{
    SDL_ASSERT(s.is_valid());      
    const quadrant quad = longitude_quadrant(s.longitude);
    const double meridian = longitude_meridian(s.longitude, quad);
    SDL_ASSERT((meridian >= 0) && (meridian <= 90));    
    const point_3D p3 = line_plane_intersect((hemisphere::north == h) ? s.latitude : -s.latitude, meridian);
    return scale_plane_intersect(p3, quad, h);
}

spatial_point math::reverse_project_globe(point_2D const & p2)
{
    const quadrant quad = point_quadrant(p2);
    const hemisphere is_north = point_hemisphere(p2);
    const point_3D p3 = reverse_scale_plane_intersect(p2, quad, is_north);
    spatial_point ret = reverse_line_plane_intersect(p3);
    if (is_north != hemisphere::north) {
        ret.latitude *= -1;
    }
    if (fequal(a_abs(ret.latitude), 90)) {
        ret.longitude = 0;
    }
    else {
        ret.longitude = reverse_longitude_meridian(ret.longitude, quad);
    }
    SDL_ASSERT(ret.is_valid());
    return ret;
}

spatial_cell math::make_cell(XY const & p_0, spatial_grid const grid)
{
    using namespace globe_to_cell_;
    const int s_0 = grid.s_0();
    const int s_1 = grid.s_1();
    const int s_2 = grid.s_2();
    SDL_ASSERT(p_0.X >= 0);
    SDL_ASSERT(p_0.Y >= 0);
    SDL_ASSERT(p_0.X < grid.s_3());
    SDL_ASSERT(p_0.Y < grid.s_3());
    const XY h_0 = div_XY(p_0, s_2);
    const XY p_1 = mod_XY(p_0, h_0, s_2);
    const XY h_1 = div_XY(p_1, s_1);
    const XY p_2 = mod_XY(p_1, h_1, s_1);
    const XY h_2 = div_XY(p_2, s_0);
    const XY h_3 = mod_XY(p_2, h_2, s_0);
    SDL_ASSERT((h_0.X >= 0) && (h_0.X < grid[0]));
    SDL_ASSERT((h_0.Y >= 0) && (h_0.Y < grid[0]));
    SDL_ASSERT((h_1.X >= 0) && (h_1.X < grid[1]));
    SDL_ASSERT((h_1.Y >= 0) && (h_1.Y < grid[1]));
    SDL_ASSERT((h_2.X >= 0) && (h_2.X < grid[2]));
    SDL_ASSERT((h_2.Y >= 0) && (h_2.Y < grid[2]));
    SDL_ASSERT((h_3.X >= 0) && (h_3.X < grid[3]));
    SDL_ASSERT((h_3.Y >= 0) && (h_3.Y < grid[3]));
    spatial_cell cell; // uninitialized
    cell[0] = hilbert::xy2d<spatial_cell::id_type>(grid[0], h_0); // hilbert curve distance 
    cell[1] = hilbert::xy2d<spatial_cell::id_type>(grid[1], h_1);
    cell[2] = hilbert::xy2d<spatial_cell::id_type>(grid[2], h_2);
    cell[3] = hilbert::xy2d<spatial_cell::id_type>(grid[3], h_3);
    cell.data.depth = 4;
    return cell;
}

spatial_cell math::globe_to_cell(const point_2D & globe, spatial_grid const grid)
{
    using namespace globe_to_cell_;

    const int g_0 = grid[0];
    const int g_1 = grid[1];
    const int g_2 = grid[2];
    const int g_3 = grid[3];

    SDL_ASSERT_1(frange(globe.X, 0, 1));
    SDL_ASSERT_1(frange(globe.Y, 0, 1));

    const point_XY<int> h_0 = min_max(scale(g_0, globe), g_0 - 1);
    const point_2D fraction_0 = fraction(globe, h_0, g_0);

    SDL_ASSERT_1(frange(fraction_0.X, 0, 1));
    SDL_ASSERT_1(frange(fraction_0.Y, 0, 1));

    const point_XY<int> h_1 = min_max(scale(g_1, fraction_0), g_1 - 1);    
    const point_2D fraction_1 = fraction(fraction_0, h_1, g_1);

    SDL_ASSERT_1(frange(fraction_1.X, 0, 1));
    SDL_ASSERT_1(frange(fraction_1.Y, 0, 1));

    const point_XY<int> h_2 = min_max(scale(g_2, fraction_1), g_2 - 1);    
    const point_2D fraction_2 = fraction(fraction_1, h_2, g_2);

    SDL_ASSERT_1(frange(fraction_2.X, 0, 1));
    SDL_ASSERT_1(frange(fraction_2.Y, 0, 1));

    const point_XY<int> h_3 = min_max(scale(g_3, fraction_2), g_3 - 1);
    spatial_cell cell; // uninitialized
    cell[0] = hilbert::xy2d<spatial_cell::id_type>(g_0, h_0); // hilbert curve distance 
    cell[1] = hilbert::xy2d<spatial_cell::id_type>(g_1, h_1);
    cell[2] = hilbert::xy2d<spatial_cell::id_type>(g_2, h_2);
    cell[3] = hilbert::xy2d<spatial_cell::id_type>(g_3, h_3);
    cell.data.depth = 4;
    return cell;
}

/*
https://en.wikipedia.org/wiki/Haversine_formula
http://www.movable-type.co.uk/scripts/gis-faq-5.1.html
Haversine Formula (from R.W. Sinnott, "Virtues of the Haversine", Sky and Telescope, vol. 68, no. 2, 1984, p. 159):
dlon = lon2 - lon1
dlat = lat2 - lat1
a = sin^2(dlat/2) + cos(lat1) * cos(lat2) * sin^2(dlon/2)
c = 2 * arcsin(min(1,sqrt(a)))
d = R * c 
The great circle distance d will be in the same units as R */
Meters math::haversine(spatial_point const & _1, spatial_point const & _2, const Meters R)
{
    const double dlon = limits::DEG_TO_RAD * (_2.longitude - _1.longitude);
    const double dlat = limits::DEG_TO_RAD * (_2.latitude - _1.latitude);
    const double sin_lat = sin(dlat / 2);
    const double sin_lon = sin(dlon / 2);
    const double a = sin_lat * sin_lat + 
        cos(limits::DEG_TO_RAD * _1.latitude) * 
        cos(limits::DEG_TO_RAD * _2.latitude) * sin_lon * sin_lon;
    const double c = 2 * asin(a_min(1.0, sqrt(a)));
    return c * R.value();
}

/*
http://www.movable-type.co.uk/scripts/latlong.html
http://williams.best.vwh.net/avform.htm#LL
Destination point given distance and bearing from start point
Given a start point, initial bearing, and distance, 
this will calculate the destination point and final bearing travelling along a (shortest distance) great circle arc.
var lat2 = Math.asin( Math.sin(lat1)*Math.cos(d/R) + Math.cos(lat1)*Math.sin(d/R)*Math.cos(brng) );
var lon2 = lon1 + Math.atan2(Math.sin(brng)*Math.sin(d/R)*Math.cos(lat1), Math.cos(d/R)-Math.sin(lat1)*Math.sin(lat2)); */
spatial_point math::destination(spatial_point const & p, Meters const distance, Degree const bearing)
{
    SDL_ASSERT(frange(bearing.value(), 0, 360)); // clockwize direction to north [0..360]
    if (distance.value() <= 0) {
        return p;
    }
    const double radius = earth_radius(p.latitude); // in meters    
    const double dist = distance.value() / radius; // angular distance in radians
    const double brng = bearing.value() * limits::DEG_TO_RAD;
    const double lat1 = p.latitude * limits::DEG_TO_RAD;
    const double lon1 = p.longitude * limits::DEG_TO_RAD;
    const double lat2 = std::asin(std::sin(lat1) * std::cos(dist) + std::cos(lat1) * std::sin(dist) * std::cos(brng));
    const double x = std::cos(dist) - std::sin(lat1) * std::sin(lat2);
    const double y = std::sin(brng) * std::sin(dist) * std::cos(lat1);
    const double lon2 = lon1 + fatan2(y, x);
    spatial_point dest;
    dest.latitude = norm_latitude(lat2 * limits::RAD_TO_DEG);
    dest.longitude = latitude_pole(p.latitude) ?
        norm_longitude(bearing.value()) : // pole is special/rare case
        norm_longitude(lon2 * limits::RAD_TO_DEG);
    SDL_ASSERT(dest.is_valid());
    return dest;
}

point_XY<int> math::quadrant_grid(quadrant const quad, int const grid) {
    SDL_ASSERT(quad <= 3);
    point_XY<int> size;
    if (quad & 1) { // 1, 3
        size.X = grid;
        size.Y = grid / 4;
    }
    else {
        size.X = grid / 2;
        size.Y = grid / 2;
    }
    return size;
}

math::quadrant math::point_quadrant(point_2D const & p) {
    const bool is_north = (p.Y >= 0.5);
    point_2D const pole{ 0.5, is_north ? 0.75 : 0.25 };
    point_2D const vec { p.X - pole.X, p.Y - pole.Y };
    double arg = polar(vec).arg; // in radians
    if (!is_north) {
        arg *= -1.0;
    }
    if (arg >= 0) {
        if (arg <= limits::ATAN_1_2)
            return q_0; 
        if (arg <= limits::PI - limits::ATAN_1_2)
            return q_1; 
    }
    else {
        if (arg >= -limits::ATAN_1_2)
            return q_0; 
        if (arg >= limits::ATAN_1_2 - limits::PI)
            return q_3;
    }
    return q_2;
}

bool math::destination_rect(spatial_rect & rc, spatial_point const & where, Meters const radius) {
    const double degree = limits::RAD_TO_DEG * radius.value() / math::earth_radius(where.latitude);
    rc.min_lat = add_latitude(where.latitude, -degree);
    rc.max_lat = add_latitude(where.latitude, degree);
    rc.min_lon = destination(where, radius, Degree(270)).longitude; // left direction
    rc.max_lon = destination(where, radius, Degree(90)).longitude; // right direction
    if ((rc.max_lat != (where.latitude + degree)) ||
        (rc.min_lat != (where.latitude - degree))) {
        return false; // returns false if wrap over pole
    }
    SDL_ASSERT(rc);
    SDL_ASSERT(where.latitude > rc.min_lat);
    SDL_ASSERT(where.latitude < rc.max_lat);
    return true;
}

bool math::rect_cross_quadrant(spatial_rect const & rc) {
    for (size_t i = 0; i < 4; ++i) {
        if (math::cross_longitude(sorted_quadrant[i], rc.min_lon, rc.max_lon)) {
            return true;
        }
    }
    return false;
}

bool math::cross_longitude(double mid, double left, double right) {
    SDL_ASSERT(SP::valid_longitude(mid));
    SDL_ASSERT(SP::valid_longitude(left));
    SDL_ASSERT(SP::valid_longitude(right));
    if (mid < 0) mid += 360;
    if (left < 0) left += 360;
    if (right < 0) right += 360;
    if (left <= right) {
        return (left < mid) && (mid < right);
    }
    return (left < mid) || (mid < right);
}

double math::longitude_distance(double left, double right) {
    SDL_ASSERT(SP::valid_longitude(left));
    SDL_ASSERT(SP::valid_longitude(right));
    if (left < 0) left += 360;
    if (right < 0) right += 360;
    if (left <= right) {
        return right - left;
    }
    return 360 - (left - right);
}

void math::poly_latitude(buf_2D & dest,
                        double const lat, 
                        double const _lon1,
                        double const _lon2,
                        hemisphere const h,
                        bool const change_direction) { // with first and last points
    SDL_ASSERT(_lon1 != _lon2);
    double const lon1 = change_direction ? _lon2 : _lon1;
    double const lon2 = change_direction ? _lon1 : _lon2;
    double const ld = longitude_distance(_lon1, _lon2, change_direction);
    SDL_ASSERT_1(change_direction ? frange(ld, -90, 0) : frange(ld, 0, 90));
    spatial_point const p1 = SP::init(Latitude(lat), Longitude(lon1));
    spatial_point const p2 = SP::init(Latitude(lat), Longitude(lon2));
    Meters const distance = math::haversine(p1, p2);
    enum { min_num = 3 }; // must be odd    
    size_t const num = min_num + static_cast<size_t>(distance.value() / 100000) * 2; //FIXME: experimental, must be odd
    double const step = ld / (num + 1);
    dest.push_back(project_globe(p1, h));
    SP mid;
    mid.latitude = lat;
    for (size_t i = 1; i <= num; ++i) {
        mid.longitude = add_longitude(lon1, step * i);
        dest.push_back(project_globe(mid, h));
    }
    dest.push_back(project_globe(p2, h));
}

void math::poly_longitude(buf_2D & dest,
                        double const lon, 
                        double const _lat1,
                        double const _lat2,
                        hemisphere const h,
                        bool const change_direction) { // without first and last points
    SDL_ASSERT(_lat1 != _lat2);
    double const lat1 = change_direction ? _lat2 : _lat1;
    double const lat2 = change_direction ? _lat1 : _lat2;
    double const ld = lat2 - lat1;
    spatial_point const p1 = SP::init(Latitude(lat1), Longitude(lon));
    spatial_point const p2 = SP::init(Latitude(lat2), Longitude(lon));
    Meters const distance = math::haversine(p1, p2);
    enum { min_num = 3 }; // must be odd    
    size_t const num = min_num + static_cast<size_t>(distance.value() / 100000) * 2; //FIXME: experimental, must be odd
    double const step = ld / (num + 1);
    SP mid;
    mid.longitude = lon;
    for (size_t i = 1; i <= num; ++i) {
        mid.latitude = lat1 + step * i;
        dest.push_back(project_globe(mid, h));
    }
}

void math::poly_rect(buf_2D & dest, spatial_rect const & rc, hemisphere const h)
{
#if use_fill_poly_without_plot_line
    poly_latitude(dest, rc.min_lat, rc.min_lon, rc.max_lon, h, false);
    poly_longitude(dest, rc.max_lon, rc.min_lat, rc.max_lat, h, false); // not optimized
    poly_latitude(dest, rc.max_lat, rc.min_lon, rc.max_lon, h, true);
    poly_longitude(dest, rc.min_lon, rc.min_lat, rc.max_lat, h, true); // not optimized
#else
    poly_latitude(dest, rc.min_lat, rc.min_lon, rc.max_lon, h, false);
    poly_latitude(dest, rc.max_lat, rc.min_lon, rc.max_lon, h, true);
#endif
}

void math::select_sector(interval_cell & result, spatial_rect const & rc, spatial_grid const grid)
{
    SDL_ASSERT(rc && !rc.cross_equator() && !rect_cross_quadrant(rc));
    SDL_ASSERT(fless_eq(longitude_distance(rc.min_lon, rc.max_lon), 90));
    SDL_ASSERT(longitude_quadrant(rc.min_lon) <= longitude_quadrant(rc.max_lon));
    const hemisphere h = latitude_hemisphere((rc.min_lat + rc.max_lat) / 2);
    buf_2D verts;
    poly_rect(verts, rc, h);
    fill_poly(result, verts, grid);
}

void math::select_hemisphere(interval_cell & result, spatial_rect const & rc, spatial_grid const grid)
{
    SDL_ASSERT(rc && !rc.cross_equator());
    spatial_rect sector = rc;
    for (size_t i = 0; i < quadrant_size; ++i) {
        double const d = math::sorted_quadrant[i];
        SDL_ASSERT((0 == i) || (math::sorted_quadrant[i - 1] < d));
        if (cross_longitude(d, sector.min_lon, sector.max_lon)) {
            SDL_ASSERT(d != sector.min_lon);
            SDL_ASSERT(d != sector.max_lon);
            sector.max_lon = d;
            select_sector(result, sector, grid);
            sector.min_lon = d;
            sector.max_lon = rc.max_lon;
        }
    }
    SDL_ASSERT(sector && (sector.max_lon == rc.max_lon));
    select_sector(result, sector, grid);
}

void math::poly_range(sector_indexes & cross, buf_2D & result, 
                      spatial_point const & where, Meters const radius, 
                      sector_t const & where_sec, spatial_grid const grid)
{
    SDL_ASSERT(radius.value() > 0);
    SDL_ASSERT(where_sec == spatial_sector(where));
    SDL_ASSERT(result.empty());
    SDL_ASSERT(cross.empty());

    enum { min_num = 32 };
    const double degree = limits::RAD_TO_DEG * radius.value() / earth_radius(where);
    const size_t num = math::roundup(degree * 32, min_num); //FIXME: experimental
    SDL_ASSERT(num && !(num % min_num));
    const double bx = 360.0 / num;
    SDL_ASSERT(frange(bx, 1.0, 360.0 / min_num));

    spatial_point sp = destination(where, radius, Degree(0)); // bearing = 0
    sector_t sec1 = spatial_sector(sp), sec2;
    result.push_back(project_globe(sp));
    if (sec1.h != where_sec.h) {
        cross.push_back(sector_index{sec1, result.size() - 1});
    }
    spatial_point mid;
    point_2D next;
    for (double bearing = bx; bearing < 360; bearing += bx) { //FIXME: improve intersection accuracy
        sp = destination(where, radius, Degree(bearing));
        next = project_globe(sp);
        if ((sec2 = spatial_sector(sp)) != sec1) {
            mid = destination(where, radius, Degree(bearing - bx * 0.5)); // half back
            if (sec1.h != sec2.h) { // find intersection with equator
                mid.latitude = 0;
                SDL_WARNING_DEBUG_2(haversine_error(where, mid, radius).value() < 100); // error must be small
                result.push_back(project_globe(mid, sec1.h));
                cross.push_back(sector_index{sec2, result.size() - 1});
            }
            else { // intersection with quadrant
                SDL_ASSERT(sec1.q != sec2.q);
                SDL_WARNING_DEBUG_2(haversine_error(where, mid, radius).value() < 100); // error must be small
                result.push_back(project_globe(mid, sec1.h));
            }
            sec1 = sec2;
        }
        result.push_back(next);
    }
}

void math::rasterization(buf_XY & dest, buf_2D const & src, spatial_grid const grid)
{
    SDL_ASSERT(dest.empty());
    SDL_ASSERT(!src.empty());
    const int max_id = grid.s_3();
    XY val;
    for (auto const & p : src) {
        val = rasterization(p, max_id);
        if (dest.empty() || (val != dest.back())) {
            dest.push_back(val);
        }
    }
}

#if SDL_DEBUG > 1
void debug_fill_poly_v2i_n(
        const int xmin, const int ymin, const int xmax, const int ymax,
        const int verts[][2], const int nr,
        void (*callback)(int, int, void *), void *userData)
{
	/* originally by Darel Rex Finley, 2007 */

	int  nodes, pixel_y, i, j;
	int *node_x = (int *) std::malloc(sizeof(*node_x) * (size_t)(nr + 1));

	/* Loop through the rows of the image. */
	for (pixel_y = ymin; pixel_y < ymax; pixel_y++) {

		/* Build a list of nodes. */
		nodes = 0; j = nr - 1;
		for (i = 0; i < nr; i++) {
			if ((verts[i][1] < pixel_y && verts[j][1] >= pixel_y) ||
			    (verts[j][1] < pixel_y && verts[i][1] >= pixel_y))
			{
				node_x[nodes++] = (int)(verts[i][0] +
				                        ((double)(pixel_y - verts[i][1]) / (verts[j][1] - verts[i][1])) *
				                        (verts[j][0] - verts[i][0]));
			}
			j = i;
		}

		/* Sort the nodes, via a simple "Bubble" sort. */
		i = 0;
		while (i < nodes - 1) {
			if (node_x[i] > node_x[i + 1]) {
				std::swap(node_x[i], node_x[i + 1]);
				if (i) i--;
			}
			else {
				i++;
			}
		}

		/* Fill the pixels between node pairs. */
		for (i = 0; i < nodes; i += 2) {
			if (node_x[i] >= xmax) break;
			if (node_x[i + 1] >  xmin) {
				if (node_x[i    ] < xmin) node_x[i    ] = xmin;
				if (node_x[i + 1] > xmax) node_x[i + 1] = xmax;
				for (j = node_x[i]; j < node_x[i + 1]; j++) {
					callback(j - xmin, pixel_y - ymin, userData);
				}
			}
		}
	}
	std::free(node_x);
}
/* https://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
inline bool ray_crossing(point_2D const & test, point_2D const & p1, point_2D const & p2) {
    return ((p1.Y > test.Y) != (p2.Y > test.Y)) &&
        ((test.X + limits::fepsilon) < ((test.Y - p2.Y) * (p1.X - p2.X) / (p1.Y - p2.Y) + p2.X));
}
*/
#endif // #if SDL_DEBUG > 1

template<class fun_type>
void plot_line(point_2D const & p1, point_2D const & p2, const int max_id, fun_type set_pixel)
{
    //http://members.chello.at/~easyfilter/bresenham.c

    using namespace globe_to_cell_;    
    int x0 = min_max(max_id * p1.X, max_id - 1);
    int y0 = min_max(max_id * p1.Y, max_id - 1);
    const int x1 = min_max(max_id * p2.X, max_id - 1);
    const int y1 = min_max(max_id * p2.Y, max_id - 1);   
    int dx = a_abs(x1 - x0);
    int dy = -a_abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;    
    int err = dx + dy, e2;  // error value e_xy

    for (;;) { // not including last point
        set_pixel(x0, y0);
        e2 = 2 * err;                                   
        if (e2 >= dy) {             // e_xy + e_x > 0
            if (x0 == x1) break;                       
            err += dy; x0 += sx;                       
        }
        if (e2 <= dx) {             // e_xy + e_y < 0
            if (y0 == y1) break;
            err += dx; y0 += sy;
        }
    }        
}

#if use_fill_poly_without_plot_line 
inline void math::fill_poly(interval_cell & result, buf_2D const & verts_2D, spatial_grid const grid)
{
    fill_poly_without_plot_line(result, verts_2D, grid);
}
#else

void math::fill_internal_area(interval_cell & result, buf_XY & verts, spatial_grid const grid)
{
    SDL_ASSERT(!verts.empty());
    rect_XY rc;
    math_util::get_bbox(rc, verts.begin(), verts.end());
    vector_buf<int, 16> node_x;
    size_t nodes;
    const size_t nr = verts.size();
    for (int pixel_y = rc.lt.Y; pixel_y <= rc.rb.Y; ++pixel_y) {
        SDL_ASSERT(node_x.empty());
        size_t j = nr - 1;
        for (size_t i = 0; i < nr; j = i++) { // Build a list of nodes
            XY const & p1 = verts[j];
            XY const & p2 = verts[i];
            if ((p1.Y > pixel_y) != (p2.Y > pixel_y)) {
                const int x = static_cast<int>(p2.X + ((double)(pixel_y - p2.Y)) * (p1.X - p2.X) / (p1.Y - p2.Y));
                SDL_ASSERT(x < grid.s_3());
                node_x.push_sorted(x);
            }
        }
        SDL_ASSERT(!is_odd(node_x.size()));
        SDL_ASSERT(std::is_sorted(node_x.cbegin(), node_x.cend()));
        if ((nodes = node_x.size()) > 1) {
            const int * p = node_x.data();
            const int * const last = p + nodes - 1;
            while (p < last) {
                int const x1 = *p++;
                int const x2 = *p++;
                SDL_ASSERT(x1 <= x2);
                for (int pixel_x = x1 + 1; pixel_x < x2; ++pixel_x) { // fill internal area
                    result.insert(math::make_cell({pixel_x, pixel_y}, grid));
                }
            }
            SDL_ASSERT(p == last + 1);
            node_x.clear();
        }
    }
}

void math::fill_poly(interval_cell & result, buf_2D const & verts_2D, spatial_grid const grid)
{
    SDL_ASSERT(!verts_2D.empty());
    buf_XY verts; // contour rasterization
    { // plot contour
        size_t j = verts_2D.size() - 1;
        enum { scale_id = 4 }; // experimental
        const int max_id = grid.s_3() * scale_id; // 65536 * 4 = 262144
        XY old_point { -1, -1 };
        for (size_t i = 0; i < verts_2D.size(); j = i++) {
            point_2D const & p1 = verts_2D[j];
            point_2D const & p2 = verts_2D[i];
            { // plot_line(p1, p2)
                using namespace globe_to_cell_;    
                int x0 = min_max(max_id * p1.X, max_id - 1);
                int y0 = min_max(max_id * p1.Y, max_id - 1);
                //const int start_x0 = x0; (void)start_x0;
                //const int start_y0 = y0; (void)start_y0;
                const int x1 = min_max(max_id * p2.X, max_id - 1);
                const int y1 = min_max(max_id * p2.Y, max_id - 1);   
                int dx = a_abs(x1 - x0);
                int dy = -a_abs(y1 - y0);
                const int sx = (x0 < x1) ? 1 : -1;
                const int sy = (y0 < y1) ? 1 : -1;    
                int err = dx + dy, e2;  // error value e_xy
                XY point;
                for (;;) {
                    point.X = x0 / scale_id;
                    point.Y = y0 / scale_id;
                    SDL_ASSERT(point.X < grid.s_3());
                    SDL_ASSERT(point.Y < grid.s_3());
                    if ((point.X != old_point.X) || (point.Y != old_point.Y)) {
                        verts.push_back(point);
                        result.insert(make_cell(point, grid));
                        old_point = point;
                    }
                    e2 = 2 * err;                                   
                    if (e2 >= dy) {             // e_xy + e_x > 0
                        if (x0 == x1) break;                       
                        err += dy; x0 += sx;                       
                    }
                    if (e2 <= dx) {             // e_xy + e_y < 0
                        if (y0 == y1) break;
                        err += dx; y0 += sy;
                    }
                }        

            }
        }
    }
    SDL_ASSERT(!verts.empty());
    SDL_ASSERT(!result.empty());
    fill_internal_area(result, verts, grid);
}
#endif

void math::fill_poly_without_plot_line(interval_cell & result, buf_2D const & verts_2D, spatial_grid const grid)
{
    SDL_ASSERT(!verts_2D.empty());
    buf_XY verts;
    rasterization(verts, verts_2D, grid);
    const size_t nr = verts.size();
    for (auto const & p : verts) {
        result.insert(make_cell(p, grid));
    }
    rect_XY rc;
    math_util::get_bbox(rc, verts.begin(), verts.end());
    vector_buf<int, 16> node_x;
    size_t nodes;
    for (int pixel_y = rc.lt.Y; pixel_y <= rc.rb.Y; ++pixel_y) {
        SDL_ASSERT(node_x.empty());
        size_t j = nr - 1;
        for (size_t i = 0; i < nr; j = i++) { // Build a list of nodes
            XY const & p1 = verts[j];
            XY const & p2 = verts[i];
            if ((p1.Y > pixel_y) != (p2.Y > pixel_y)) {
                const int x = static_cast<int>(p2.X + ((double)(pixel_y - p2.Y)) * (p1.X - p2.X) / (p1.Y - p2.Y));
                SDL_ASSERT(x < grid.s_3());
                node_x.push_back(x);
            }
        }
        SDL_ASSERT(!is_odd(node_x.size()));
        if ((nodes = node_x.size()) > 1) {
            node_x.sort();
            const int * p = node_x.data();
            const int * const last = p + nodes - 1;
            while (p < last) {
                int const x1 = *p++;
                int const x2 = *p++;
                SDL_ASSERT(x1 <= x2);
                for (int pixel_x = x1; pixel_x <= x2; ++pixel_x) {
                    result.insert(math::make_cell({pixel_x, pixel_y}, grid));
                }
            }
            SDL_ASSERT(p == last + 1);
            node_x.clear();
        }
    }
    SDL_ASSERT(!result.empty());
}

void math::select_range(interval_cell & result, spatial_point const & where, Meters const radius, spatial_grid const grid)
{
    SDL_ASSERT(result.empty());
    sector_indexes cross;
    buf_2D verts;
    sector_t const where_sec = spatial_sector(where);
    poly_range(cross, verts, where, radius, where_sec, grid);
    if (cross.empty()) {
        fill_poly(result, verts, grid);
    }
    else { // cross hemisphere
    }
}

} // space
} // db
} // sdl

#if SDL_DEBUG
namespace sdl { namespace db { namespace space {
    namespace {
            class unit_test {
            public:
                unit_test()
                {
                    test_hilbert();
                    test_spatial();
                    {
                        A_STATIC_ASSERT_TYPE(point_2D::type, double);
                        A_STATIC_ASSERT_TYPE(point_3D::type, double);                        
                        SDL_ASSERT_1(math::cartesian(Latitude(0), Longitude(0)) == point_3D{1, 0, 0});
                        SDL_ASSERT_1(math::cartesian(Latitude(0), Longitude(90)) == point_3D{0, 1, 0});
                        SDL_ASSERT_1(math::cartesian(Latitude(90), Longitude(0)) == point_3D{0, 0, 1});
                        SDL_ASSERT_1(math::cartesian(Latitude(90), Longitude(90)) == point_3D{0, 0, 1});
                        SDL_ASSERT_1(math::cartesian(Latitude(45), Longitude(45)) == point_3D{0.5, 0.5, 0.70710678118654752440});
                        SDL_ASSERT_1(math::line_plane_intersect(Latitude(0), Longitude(0)) == point_3D{1, 0, 0});
                        SDL_ASSERT_1(math::line_plane_intersect(Latitude(0), Longitude(90)) == point_3D{0, 1, 0});
                        SDL_ASSERT_1(math::line_plane_intersect(Latitude(90), Longitude(0)) == point_3D{0, 0, 1});
                        SDL_ASSERT_1(math::line_plane_intersect(Latitude(90), Longitude(90)) == point_3D{0, 0, 1});
                        SDL_ASSERT_1(fequal(length(math::line_plane_intersect(Latitude(45), Longitude(45))), 0.58578643762690497));
                        SDL_ASSERT_1(math::longitude_quadrant(0) == 0);
                        SDL_ASSERT_1(math::longitude_quadrant(45) == 1);
                        SDL_ASSERT_1(math::longitude_quadrant(90) == 1);
                        SDL_ASSERT_1(math::longitude_quadrant(135) == 2);
                        SDL_ASSERT_1(math::longitude_quadrant(180) == 2);
                        SDL_ASSERT_1(math::longitude_quadrant(-45) == 0);
                        SDL_ASSERT_1(math::longitude_quadrant(-90) == 3);
                        SDL_ASSERT_1(math::longitude_quadrant(-135) == 3);
                        SDL_ASSERT_1(math::longitude_quadrant(-180) == 2);
                        SDL_ASSERT(fequal(limits::ATAN_1_2, std::atan2(1, 2)));
#if !defined(SDL_VISUAL_STUDIO_2013)
                        static_assert(fsign(0) == 0, "");
                        static_assert(fsign(1) == 1, "");
                        static_assert(fsign(-1) == -1, "");
                        static_assert(fzero(0), "");
                        static_assert(fzero(limits::fepsilon), "");
                        static_assert(!fzero(limits::fepsilon * 2), "");
                        static_assert(a_min_max(0.5, 0.0, 1.0) == 0.5, "");
                        static_assert(a_min_max(-1.0, 0.0, 1.0) == 0.0, "");
                        static_assert(a_min_max(2.5, 0.0, 1.0) == 1.0, "");
                        static_assert(reverse_bytes(0x01020304) == 0x04030201, "reverse_bytes");
#endif
                    }
                    if (1)
                    {
                        spatial_cell x{}, y{};
                        SDL_ASSERT(!spatial_cell::less(x, y));
                        SDL_ASSERT(x == y);
                        y.set_depth(1);
                        SDL_ASSERT(x != y);
                        SDL_ASSERT(spatial_cell::less(x, y));
                        SDL_ASSERT(!spatial_cell::less(y, x));
                    }
                    if (0) { // generate static tables
                        std::cout << "\nd2xy:\n";
                        enum { HIGH = spatial_grid::HIGH };
                        int dist[HIGH][HIGH]{};
                        for (int i = 0; i < HIGH; ++i) {
                            for (int j = 0; j < HIGH; ++j) {
                                const int d = i * HIGH + j;
                                point_XY<int> const h = hilbert::d2xy(d, (spatial_cell::id_type) HIGH);
                                dist[h.X][h.Y] = d;
                                std::cout << "{" << h.X << "," << h.Y << "},";
                            }
                            std::cout << " // " << i << "\n";
                        }
                        std::cout << "\nxy2d:";
                        for (int x = 0; x < HIGH; ++x) {
                            std::cout << "\n{";
                            for (int y = 0; y < HIGH; ++y) {
                                if (y) std::cout << ",";
                                std::cout << dist[x][y];
                            }
                            std::cout << "}, // " << x;
                        }
                        std::cout << std::endl;
                    }
                    if (1) {
                        SDL_ASSERT(fequal(math::norm_longitude(0), 0));
                        SDL_ASSERT(fequal(math::norm_longitude(180), 180));
                        SDL_ASSERT(fequal(math::norm_longitude(-180), -180));
                        SDL_ASSERT(fequal(math::norm_longitude(-180 - 90), 90));
                        SDL_ASSERT(fequal(math::norm_longitude(180 + 90), -90));
                        SDL_ASSERT(fequal(math::norm_longitude(180 + 90 + 360), -90));
                        SDL_ASSERT(fequal(math::norm_latitude(0), 0));
                        SDL_ASSERT(fequal(math::norm_latitude(-90), -90));
                        SDL_ASSERT(fequal(math::norm_latitude(90), 90));
                        SDL_ASSERT(fequal(math::norm_latitude(90+10), 80));
                        SDL_ASSERT(fequal(math::norm_latitude(90+10+360), 80));
                        SDL_ASSERT(fequal(math::norm_latitude(-90-10), -80));
                        SDL_ASSERT(fequal(math::norm_latitude(-90-10-360), -80));
                        SDL_ASSERT(fequal(math::norm_latitude(-90-10+360), -80));
                    }
                    if (1) {
                        SDL_ASSERT(math::point_quadrant(point_2D{}) == 1);
                        SDL_ASSERT(math::point_quadrant(point_2D{0, 0.25}) == 2);
                        SDL_ASSERT(math::point_quadrant(point_2D{0.5, 0.375}) == 3);
                        SDL_ASSERT(math::point_quadrant(point_2D{0.5, 0.5}) == 3);
                        SDL_ASSERT(math::point_quadrant(point_2D{1.0, 0.25}) == 0);
                        SDL_ASSERT(math::point_quadrant(point_2D{1.0, 0.75}) == 0);
                        SDL_ASSERT(math::point_quadrant(point_2D{1.0, 1.0}) == 0);
                        SDL_ASSERT(math::point_quadrant(point_2D{0.5, 1.0}) == 1);
                        SDL_ASSERT(math::point_quadrant(point_2D{0, 0.75}) == 2);
                    }
                    if (1) {
                        {
                            double const earth_radius = math::earth_radius(Latitude(0)); // depends on EARTH_ELLIPSOUD
                            Meters const d1 = earth_radius * limits::PI / 2;
                            Meters const d2 = d1.value() / 2;
                            SDL_ASSERT(math::destination(SP::init(Latitude(0), Longitude(0)), d1, Degree(0)).equal(Latitude(90), Longitude(0)));
                            SDL_ASSERT(math::destination(SP::init(Latitude(0), Longitude(0)), d1, Degree(360)).equal(Latitude(90), Longitude(0)));
                            SDL_ASSERT(math::destination(SP::init(Latitude(0), Longitude(0)), d2, Degree(0)).equal(Latitude(45), Longitude(0)));
                            SDL_ASSERT(math::destination(SP::init(Latitude(0), Longitude(0)), d2, Degree(90)).equal(Latitude(0), Longitude(45)));
                            SDL_ASSERT(math::destination(SP::init(Latitude(0), Longitude(0)), d2, Degree(180)).equal(Latitude(-45), Longitude(0)));
                            SDL_ASSERT(math::destination(SP::init(Latitude(0), Longitude(0)), d2, Degree(270)).equal(Latitude(0), Longitude(-45)));
                        }
                        {
                            double const earth_radius = math::earth_radius(Latitude(90)); // depends on EARTH_ELLIPSOUD
                            Meters const d1 = earth_radius * limits::PI / 2;
                            Meters const d2 = d1.value() / 2;
                            SDL_ASSERT(math::destination(SP::init(Latitude(90), Longitude(0)), d2, Degree(0)).equal(Latitude(45), Longitude(0)));
                            SDL_ASSERT(math::destination(SP::init(Latitude(-90), Longitude(0)), d2, Degree(0)).equal(Latitude(-45), Longitude(0)));
                        }
                    }
                    if (1) { // 111 km arc of circle => line chord => 1.4 meter error
                        double constexpr R = limits::EARTH_MINOR_RADIUS;    // 6356752.3142449996 meters
                        double constexpr angle = 1.0 * limits::DEG_TO_RAD;  // 0.017453292519943295 radian
                        double constexpr L = angle * R;                     // 110946.25761734448 meters
                        double const H = 2 * R * std::sin(angle / 2);       // 110944.84944925930 meters
                        double const delta = L - H;                         // 1.4081680851813871 meters
                        SDL_ASSERT(fequal(delta, 1.4081680851813871));
                    }
                    if (1) {
                        draw_grid(false);
                        reverse_grid(false);
                    }
                }
            private:
                static void trace_hilbert(const int n) {
                    for (int y = 0; y < n; ++y) {
                        std::cout << y;
                        for (int x = 0; x < n; ++x) {
                            const int d = hilbert::xy2d(n, x, y);
                            std::cout << "," << d;
                        }
                        std::cout << std::endl;
                    }
                }
                static void test_hilbert(const int n) {
                    for (int d = 0; d < (n * n); ++d) {
                        int x = 0, y = 0;
                        hilbert::d2xy(n, d, x, y);
                        SDL_ASSERT(d == hilbert::xy2d(n, x, y));
                        //SDL_TRACE("d2xy: n = ", n, " d = ", d, " x = ", x, " y = ", y);
#if is_static_hilbert
                        if (n == spatial_grid::HIGH) {
                            SDL_ASSERT(hilbert::static_d2xy[d].X == x);
                            SDL_ASSERT(hilbert::static_d2xy[d].Y == y);
                            SDL_ASSERT(hilbert::static_xy2d[x][y] == d);
                        }
#endif
                    }
                }
                static void test_hilbert() {
                    spatial_grid::grid_size const sz = spatial_grid::HIGH;
                    for (int i = 0; (1 << i) <= sz; ++i) {
                        test_hilbert(1 << i);
                    }
                }
                static void trace_cell(const spatial_cell & ) {
                    //SDL_TRACE(to_string::type(cell));
                }
                static void test_spatial(const spatial_grid & grid) {
                    if (1) {
                        spatial_point p1{}, p2{};
                        for (int i = 0; i <= 4; ++i) {
                        for (int j = 0; j <= 2; ++j) {
                            p1.longitude = 45 * i; 
                            p2.longitude = -45 * i;
                            p1.latitude = 45 * j;
                            p2.latitude = -45 * j;
                            math::project_globe(p1);
                            math::project_globe(p2);
#if high_grid_optimization
                            math::globe_make_cell(p1, spatial_grid());
#else
                            math::globe_make_cell(p1, spatial_grid(spatial_grid::LOW));
                            math::globe_make_cell(p1, spatial_grid(spatial_grid::MEDIUM));
                            math::globe_make_cell(p1, spatial_grid(spatial_grid::HIGH));
#endif
                        }}
                    }
                    if (1) {
                        static const spatial_point test[] = { // latitude, longitude
                            { 48.7139, 44.4984 },   // cell_id = 156-163-67-177-4
                            { 55.7975, 49.2194 },   // cell_id = 157-178-149-55-4
                            { 47.2629, 39.7111 },   // cell_id = 163-78-72-221-4
                            { 47.261, 39.7068 },    // cell_id = 163-78-72-223-4
                            { 55.7831, 37.3567 },   // cell_id = 156-38-25-118-4
                            { 0, -86 },             // cell_id = 128-234-255-15-4
                            { 45, -135 },           // cell_id = 70-170-170-170-4 | 73-255-255-255-4 | 118-0-0-0-4 | 121-85-85-85-4 
                            { 45, 135 },            // cell_id = 91-255-255-255-4 | 92-170-170-170-4 | 99-85-85-85-4 | 100-0-0-0-4
                            { 45, 0 },              // cell_id = 160-236-255-239-4 | 181-153-170-154-4
                            { 45, -45 },            // cell_id = 134-170-170-170-4 | 137-255-255-255-4 | 182-0-0-0-4 | 185-85-85-85-4
                            { 0, 0 },               // cell_id = 175-255-255-255-4 | 175-255-255-255-4
                            { 0, 135 },
                            { 0, 90 },
                            { 90, 0 },
                            { -90, 0 },
                            { 0, -45 },
                            { 45, 45 },
                            { 0, 180 },
                            { 0, -180 },
                            { 0, 131 },
                            { 0, 134 },
                            { 0, 144 },
                            { 0, 145 },
                            { 0, 166 },             // cell_id = 5-0-0-79-4 | 80-85-85-58-4
                        };
                        for (size_t i = 0; i < A_ARRAY_SIZE(test); ++i) {
                            //std::cout << i << ": " << to_string::type(test[i]) << " => ";
                            trace_cell(math::globe_make_cell(test[i], grid));
                        }
                    }
                    if (1) {
                        spatial_point p1 {};
                        spatial_point p2 {};
                        SDL_ASSERT(fequal(math::haversine(p1, p2).value(), 0));
                        {
                            p2.latitude = 90.0 / 16;
                            const double h1 = math::haversine(p1, p2, limits::EARTH_RADIUS).value();
                            const double h2 = p2.latitude * limits::DEG_TO_RAD * limits::EARTH_RADIUS;
                            SDL_ASSERT(fequal(h1, h2));
                        }
                        {
                            p2.latitude = 90.0;
                            const double h1 = math::haversine(p1, p2, limits::EARTH_RADIUS).value();
                            const double h2 = p2.latitude * limits::DEG_TO_RAD * limits::EARTH_RADIUS;
                            SDL_ASSERT(fless(a_abs(h1 - h2), 1e-08));
                        }
                        if (math::EARTH_ELLIPSOUD) {
                            SDL_ASSERT(fequal(math::earth_radius(0), limits::EARTH_MAJOR_RADIUS));
                            SDL_ASSERT(fequal(math::earth_radius(90), limits::EARTH_MINOR_RADIUS));
                        }
                        else {
                            SDL_ASSERT(fequal(math::earth_radius(0), limits::EARTH_RADIUS));
                            SDL_ASSERT(fequal(math::earth_radius(90), limits::EARTH_RADIUS));
                        }
                    }
                }
                static void test_spatial() {
                    test_spatial(spatial_grid());
                }
                static void draw_grid(bool const print) {
                    if (1) {
                        if (print) {
                            std::cout << "\ndraw_grid:\n";
                        }
                        const double sx = 16 * 4;
                        const double sy = 16 * 2;
                        const double dy = (SP::max_latitude - SP::min_latitude) / sy;
                        const double dx = (SP::max_longitude - SP::min_longitude) / sx;
                        size_t i = 0;
                        for (double y = SP::min_latitude; y <= SP::max_latitude; y += dy) {
                        for (double x = SP::min_longitude; x <= SP::max_longitude; x += dx) {
                            point_2D const p2 = math::project_globe(SP::init(Latitude(y), Longitude(x)));
                            if (print) {
                                std::cout << (i++) 
                                    << "," << p2.X
                                    << "," << p2.Y
                                    << "," << x
                                    << "," << y
                                    << "\n";
                            }
                            SP const g = math::reverse_project_globe(p2);
                            SDL_ASSERT(g.match(SP::init(Latitude(y), Longitude(x))));
                        }}
                    }
                    if (0) {
                        draw_circle(SP::init(Latitude(45), Longitude(0)), Meters(1000 * 1000));
                        draw_circle(SP::init(Latitude(0), Longitude(0)), Meters(1000 * 1000));
                        draw_circle(SP::init(Latitude(60), Longitude(45)), Meters(1000 * 1000));
                        draw_circle(SP::init(Latitude(85), Longitude(30)), Meters(1000 * 1000));
                        draw_circle(SP::init(Latitude(-60), Longitude(30)), Meters(1000 * 500));
                        draw_circle(SP::init(Latitude(90), Longitude(0)), Meters(100 * 1000));
                    }
                    if (print) {
                        draw_circle(SP::init(Latitude(56.3153), Longitude(44.0107)), Meters(100 * 1000));
                    }
                }
                static void draw_circle(SP const center, Meters const distance) {
                    //std::cout << "\ndraw_circle:\n";
                    const double bx = 1;
                    size_t i = 0;
                    for (double bearing = 0; bearing < 360; bearing += bx) {
                        SP const sp = math::destination(center, distance, Degree(bearing));
                        point_2D const p = math::project_globe(sp);
                        std::cout << (i++) 
                            << "," << p.X
                            << "," << p.Y
                            << "," << sp.longitude
                            << "," << sp.latitude
                            << "\n";
                    }
                }
                static void reverse_grid(bool const print) {
                    if (print) {
                        std::cout << "\nreverse_grid:\n";
                    }
                    size_t i = 0;
                    const double d = spatial_grid().f_0() / 2.0;
                    for (double x = 0; x <= 1.0; x += d) {
                        for (double y = 0; y <= 1.0; y += d) {
                            point_2D const p1{ x, y };
                            SP const g1 = math::reverse_project_globe(p1);
                            if (print) {
                                std::cout << (i++) 
                                    << "," << p1.X
                                    << "," << p1.Y
                                    << "," << g1.longitude
                                    << "," << g1.latitude
                                    << "\n";
                            }
                            if (!fzero(g1.latitude)) {
                                point_2D const p2 = math::project_globe(g1);
                                SDL_ASSERT(p2 == p1);
                            }
                        }
                    }
                }
            };
            static unit_test s_test;
} // namespace
} // space
} // db
} // sdl
#endif //#if SV_DEBUG