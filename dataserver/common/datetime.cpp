// datetime.cpp
//
#include "dataserver/common/datetime.h"
#include "dataserver/common/time_util.h"
#include "dataserver/utils/gregorian.hpp"

namespace sdl {

// convert to number of seconds that have elapsed since 00:00:00 UTC, 1 January 1970
size_t datetime_t::get_unix_time() const
{
    SDL_ASSERT(unix_epoch());
    size_t result = (static_cast<size_t>(this->days) - static_cast<size_t>(u_date_diff)) * day_to_sec<1>::value;
    result += static_cast<size_t>(this->ticks) / 300;
    return result;
}

gregorian_t datetime_t::gregorian() const
{
    if (is_null()) {
        return {};
    }
    using calendar = date_time::gregorian_calendar;
    using ymd_type = calendar::ymd_type;
    const auto day_number = calendar::day_number(ymd_type(1900, 1, 1)) + this->days;
    const auto ymd = calendar::from_day_number(day_number);
    gregorian_t result; //uninitialized
    result.year = ymd.year;
    result.month = ymd.month;
    result.day = ymd.day;
    return result;
}

clocktime_t datetime_t::clocktime() const
{
    if (is_null()) {
        return {};
    }
    datetime_t src;
    src.ticks = this->ticks;
    src.days = datetime_t::u_date_diff;
    struct tm src_tm;
    if (time_util::safe_gmtime(src_tm, static_cast<time_t>(src.get_unix_time()))) {
        clocktime_t result; //uninitialized
        result.hour = src_tm.tm_hour;
        result.min = src_tm.tm_min;
        result.sec = src_tm.tm_sec;
        result.milliseconds = src.milliseconds();
        return result;
    }
    SDL_ASSERT(0);
    return {};
}

#if 0
struct tm {
int tm_sec;     /* seconds after the minute - [0,59] */
int tm_min;     /* minutes after the hour - [0,59] */
int tm_hour;    /* hours since midnight - [0,23] */
int tm_mday;    /* day of the month - [1,31] */
int tm_mon;     /* months since January - [0,11] */
int tm_year;    /* years since 1900 */
int tm_wday;    /* days since Sunday - [0,6] */
int tm_yday;    /* days since January 1 - [0,365] */
int tm_isdst;   /* daylight savings time flag */
}; 
#endif
datetime_t datetime_t::set_unix_time(size_t const val)
{
    datetime_t result = {};    
    time_t temp = static_cast<time_t>(val);
    struct tm tt;
    if (time_util::safe_gmtime(tt, temp)) {
        result.days = u_date_diff + int32(val / day_to_sec<1>::value);
        result.ticks = tt.tm_sec;
        result.ticks += tt.tm_min * min_to_sec<1>::value;
        result.ticks += tt.tm_hour * hour_to_sec<1>::value;
        result.ticks *= 300;
    }
    return result;
}

datetime_t datetime_t::set_gregorian(gregorian_t const & g)
{
    datetime_t result = {};
    using calendar = date_time::gregorian_calendar;
    using ymd_type = calendar::ymd_type;
    result.days = 
        calendar::day_number(ymd_type(g.year, g.month, g.day)) - 
        calendar::day_number(ymd_type(1900, 1, 1));
    return result;
}

} // sdl

#if SDL_DEBUG
namespace sdl { namespace {
    class unit_test {
    public:
        unit_test() {
            A_STATIC_ASSERT_IS_POD(datetime_t);
            A_STATIC_ASSERT_IS_POD(gregorian_t);
            A_STATIC_ASSERT_IS_POD(clocktime_t);
            static_assert(sizeof(datetime_t) == 8, "");
            A_STATIC_ASSERT_IS_POD(datetime_t);
            static_assert(sizeof(datetime_t) == 8, "");
            A_STATIC_ASSERT_IS_POD(gregorian_t);
            static_assert(is_empty(gregorian_t{}), "gregorian_t");
            static_assert(is_valid(gregorian_t{}), "gregorian_t");
        }
    };
    static unit_test s_test;
}} // sdl
#endif //#if SDL_DEBUG

