// sparse_set.hpp
//
#pragma once
#ifndef __SDL_SPATIAL_SPARSE_SET_HPP__
#define __SDL_SPATIAL_SPARSE_SET_HPP__

namespace sdl { namespace db {

template<typename value_type>
bool sparse_set<value_type>::assert_bit_state(bit_state const & state) const {
    A_STATIC_CHECK_TYPE(const map_key, state.first.first->first); // segment (map key)
    A_STATIC_CHECK_TYPE(uint64, state.first.second->second); // mask (map value)
    A_STATIC_CHECK_TYPE(int, state.second); // bit offset in mask
    SDL_ASSERT(state.first.first != cmap().end());
    SDL_ASSERT(state.first.second == cmap().end());
    SDL_ASSERT((state.second >= 0) && (state.second < seg_size));
    SDL_ASSERT(state.first.first->second);
    SDL_ASSERT(state.first.first->second & (uint64(1) << state.second));
    SDL_ASSERT((state.first.first->second >> state.second) & 1);
    return true;
}

template<typename value_type>
bool sparse_set<value_type>::find(const value_type value, bool_constant<false>) const {
    const map_key seg = static_cast<map_key>(value / seg_size);
    const int bit = static_cast<map_key>(value & seg_mask);
    SDL_ASSERT(bit == static_cast<map_key>(value % seg_size));
    SDL_ASSERT((bit >= 0) && (bit < seg_size));
    const uint64 flag = uint64(1) << bit;
    SDL_ASSERT(flag < (uint64)(-1));
    SDL_ASSERT(value == make_value(seg, bit));
    const auto & it = cmap().find(seg);
    if (it != cmap().end()) {
        A_STATIC_CHECK_TYPE(uint64, it->second);
        return (it->second & flag) != 0;
    }
    return false;
}

template<typename value_type>
bool sparse_set<value_type>::find(const value_type value, bool_constant<true>) const {
    map_key seg;
    int bit;
    if (value < 0) {
        const value_type pos = - value - 1;
        SDL_ASSERT(pos >= 0);
        seg = - static_cast<map_key>(pos / seg_size) - 1;
        bit = seg_mask - static_cast<map_key>(pos & seg_mask);
        SDL_ASSERT(seg < 0);
        SDL_ASSERT(pos >= 0);
    }
    else {
        seg = static_cast<map_key>(value / seg_size);
        bit = static_cast<map_key>(value & seg_mask);
        SDL_ASSERT(bit == static_cast<map_key>(value % seg_size));
        SDL_ASSERT(seg >= 0);
    }
    SDL_ASSERT((bit >= 0) && (bit < seg_size));
    const uint64 flag = uint64(1) << bit;
    SDL_ASSERT(flag < (uint64)(-1));
    SDL_ASSERT(value == make_value(seg, bit));
    const auto & it = cmap().find(seg);
    if (it != cmap().end()) {
        A_STATIC_CHECK_TYPE(uint64, it->second);
        return (it->second & flag) != 0;
    }
    return false;
}

template<typename value_type>
bool sparse_set<value_type>::insert(const value_type value, bool_constant<false>) {
    const map_key seg = static_cast<map_key>(value / seg_size);
    const int bit = static_cast<map_key>(value & seg_mask);
    SDL_ASSERT(bit == static_cast<map_key>(value % seg_size));
    SDL_ASSERT((bit >= 0) && (bit < seg_size));
    const uint64 flag = uint64(1) << bit;
    SDL_ASSERT(flag < (uint64)(-1));
    SDL_ASSERT(value == make_value(seg, bit));
    uint64 & slot = map()[seg];
    if (slot & flag) {
        return false;
    }
    slot |= flag;
    ++m_size;
    return true;
}

template<typename value_type>
bool sparse_set<value_type>::insert(const value_type value, bool_constant<true>) {
    map_key seg;
    int bit;
    if (value < 0) {
        const value_type pos = - value - 1;
        SDL_ASSERT(pos >= 0);
        seg = - static_cast<map_key>(pos / seg_size) - 1;
        bit = seg_mask - static_cast<map_key>(pos & seg_mask);
        SDL_ASSERT(seg < 0);
        SDL_ASSERT(pos >= 0);
    }
    else {
        seg = static_cast<map_key>(value / seg_size);
        bit = static_cast<map_key>(value & seg_mask);
        SDL_ASSERT(bit == static_cast<map_key>(value % seg_size));
        SDL_ASSERT(seg >= 0);
    }
    SDL_ASSERT((bit >= 0) && (bit < seg_size));
    const uint64 flag = uint64(1) << bit;
    SDL_ASSERT(flag < (uint64)(-1));
    SDL_ASSERT(value == make_value(seg, bit));
    uint64 & slot = map()[seg];
    if (slot & flag) {
        return false;
    }
    slot |= flag;
    ++m_size;
    return true;
}

template<typename value_type>
template<class fun_type> break_or_continue
sparse_set<value_type>::for_each(fun_type && fun) const {
    auto const last = cmap().end();
    for(auto it = cmap().begin(); it != last; ++it) {
        A_STATIC_CHECK_TYPE(uint64, it->second);
        uint64 slot = it->second;
        SDL_ASSERT(slot);
        for (int bit = 0; slot; ++bit) {
            SDL_ASSERT(bit < seg_size);
            if (slot & 1) {
                if (is_break(fun(make_value(it->first, bit)))) {
                    return bc::break_;
                }
            }
            slot >>= 1;
        }
    }
    return bc::continue_;
}

template<typename value_type>
void sparse_set<value_type>::load_next(bit_state & state) const
{
    SDL_ASSERT(assert_bit_state(state));
    SDL_ASSERT(state.first.second == cmap().end());
    auto & first = state.first.first;
    int bit = ++(state.second);
    if (bit < seg_size) {
        uint64 slot = first->second;
        SDL_ASSERT(slot && (bit > 0));
        SDL_ASSERT((slot >> (bit - 1)) & 1);
        slot >>= bit;
        for (; slot; ++bit) {
            SDL_ASSERT(bit < seg_size);
            if (slot & 1) {
                state.second = bit;
                return;
            }
            slot >>= 1;
        }
        SDL_ASSERT(first != cmap().end());
    }
    ++first;
    state.second = 0;
    if (first != state.first.second) {
        uint64 slot = first->second;
        SDL_ASSERT(slot);
        for (; slot; ++(state.second)) {
            SDL_ASSERT(state.second < seg_size);
            if (slot & 1) {
                return;
            }
            slot >>= 1;
        }
        SDL_ASSERT(0);
    }
}

template<typename value_type>
typename sparse_set<value_type>::iterator
sparse_set<value_type>::begin() const
{
    auto const last = cmap().end();
    for(auto it = cmap().begin(); it != last; ++it) {
        A_STATIC_CHECK_TYPE(uint64, it->second);
        uint64 slot = it->second;
        SDL_ASSERT(slot);
        for (int bit = 0; slot; ++bit) {
            SDL_ASSERT(bit < seg_size);
            if (slot & 1) {
                return iterator(this, bit_state({it, last}, bit));
            }
            slot >>= 1;
        }
    }
    return this->end();
}

template<typename value_type>
typename sparse_set<value_type>::iterator
sparse_set<value_type>::end() const
{
    auto const last = cmap().end();
    return iterator(this, bit_state({last, last}, 0));
}

template<typename value_type>
std::vector<value_type>
sparse_set<value_type>::copy_to_vector() const {
    A_STATIC_ASSERT_IS_POD(value_type);
    std::vector<value_type> result(size());
    auto it = result.begin();
    for_each([&it](value_type value){
        *it++ = value;
        return bc::continue_;
    });
    SDL_ASSERT(it == result.end());
    return result;
}

template<typename T>
struct sparse_set_trait {
    using type = interval_set<T>;
};

template<> struct sparse_set_trait<int16>  { using type = sparse_set<int16>; };
template<> struct sparse_set_trait<int32>  { using type = sparse_set<int32>; };
template<> struct sparse_set_trait<int64>  { using type = sparse_set<int64>; };

template<> struct sparse_set_trait<uint16> { using type = sparse_set<uint16>; };
template<> struct sparse_set_trait<uint32> { using type = sparse_set<uint32>; };
template<> struct sparse_set_trait<uint64> { using type = sparse_set<uint64>; };

template<typename T>
using sparse_set_t = typename sparse_set_trait<T>::type;

} // db
} // sdl

#endif // __SDL_SPATIAL_SPARSE_SET_HPP__