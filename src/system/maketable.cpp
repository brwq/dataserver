// maketable.cpp
//
#include "common/common.h"
#include "maketable.h"

namespace sdl { namespace db {  namespace make {

/*tab.scan([](auto row) {});
tab.select({ if row return true; });
tab.find_pk(int[]);
tab.find_less(int, use_indexes);
tab.find_less(300).select( {bool} );
tab.find_less(300).top(10); // last(10)
tab.top(5).union( tab.last(5) );
tab.find( {bool} ) 
*/

#if SDL_DEBUG
namespace sample { namespace {
    template <class type_list> struct processor;

    template <> struct processor<NullType> {
        static void test(){}
    };
    template <class T, class U> // T = meta::col_type
    struct processor< Typelist<T, U> > {
        static void test(){
            T::test();
            processor<U>::test();
        }
    };
    void test_sample_table(sample::dbo_table * const table) {
        if (!table) return;
        using T = sample::dbo_table;
        static_assert(T::col_size == 3, "");
        static_assert(T::col_fixed, "");
        static_assert(sizeof(T::record) == 8, "");
        T & tab = *table;
        for (auto p : tab) {
            if (p.Id()) {}
        }
        tab->scan_if([](T::record){
            return true;
        });
        tab.query.scan_if([](T::record){
            return true;
        });
        if (auto found = tab.query.find([](T::record p){
            return p.Id() > 0;
        })) {
            SDL_ASSERT(found.Id() > 0);
        }
        auto range = tab.query.select([](T::record p){
            return p.Id() > 0;
        });
        using CLUSTER = T::cluster_index;
        using key_type = CLUSTER::key_type;
        static_assert(CLUSTER::index_size == 2, "");
        static_assert(sizeof(key_type) ==
            sizeof(CLUSTER::T0::type) +
            sizeof(CLUSTER::T1::type),
            "");
        static_assert(CLUSTER::index_col<0>::offset == 0, "");
        static_assert(CLUSTER::index_col<1>::offset == 4, "");
        A_STATIC_ASSERT_IS_POD(key_type);
        key_type test{};
        auto _0 = test.get<0>();
        auto _1 = test.get<1>();
        static_assert(std::is_same<int const &, decltype(test.get<0>())>::value, "");
        static_assert(std::is_same<uint64 const &, decltype(test.get<1>())>::value, "");
        test.set<0>() = _0;
        test.set<1>() = _1;
        static_assert(std::is_same<int &, decltype(test.set<0>())>::value, "");
        static_assert(std::is_same<uint64 &, decltype(test.set<1>())>::value, "");
    }
    class unit_test {
    public:
        unit_test() {
            struct col {
                using t_int                 = meta::col<0, scalartype::t_int, 4, meta::key_true>;
                using t_bigint              = meta::col<0, scalartype::t_bigint, 8>;
                using t_smallint            = meta::col<0, scalartype::t_smallint, 2>;
                using t_float               = meta::col<0, scalartype::t_float, 8>;
                using t_real                = meta::col<0, scalartype::t_real, 4>;
                using t_smalldatetime       = meta::col<0, scalartype::t_smalldatetime, 4>;
                using t_uniqueidentifier    = meta::col<0, scalartype::t_uniqueidentifier, 16>;
                using t_char                = meta::col<0, scalartype::t_char, 255>;
                using t_nchar               = meta::col<0, scalartype::t_nchar, 255>;
                using t_varchar             = meta::col<0, scalartype::t_varchar, 255>;
                //using t_geometry            = meta::col<0, scalartype::t_geometry, -1>;
                using t_geography            = meta::col<0, scalartype::t_geography>;
            };
            typedef TL::Seq<
                col::t_int
                ,col::t_bigint
                ,col::t_smallint
                ,col::t_float
                ,col::t_real
                ,col::t_smalldatetime
                ,col::t_uniqueidentifier
                ,col::t_char
                ,col::t_nchar
                ,col::t_varchar
                ,col::t_geography
            >::Type type_list;
            processor<type_list>::test();
            test_sample_table(nullptr);
            if (0) {
                SDL_TRACE(typeid(sample::dbo_META::col::Id).name());
                SDL_TRACE(typeid(sample::dbo_META::col::Col1).name());
            }
        }
    };
    static unit_test s_test;
}
#endif //#if SV_DEBUG
} // sample
} // make
} // db
} // sdl