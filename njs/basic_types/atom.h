#ifndef NJS_ATOM_H
#define NJS_ATOM_H

#include <cstdint>
#include <cassert>

namespace njs {

using u32 = uint32_t;

constexpr u32 ATOM_INT_TAG = 1u << 31;
constexpr u32 ATOM_INT_MAX = ATOM_INT_TAG - 1;
constexpr u32 ATOM_INT_MASK = ATOM_INT_MAX;
constexpr u32 ATOM_STR_SYM_MAX = UINT32_MAX >> 1;

inline bool atom_is_int(u32 atom) {
  return atom & ATOM_INT_TAG;
}

inline u32 atom_get_int(u32 atom) {
  assert(atom_is_int(atom));
  return atom & ATOM_INT_MASK;
}

inline bool atom_is_str_sym(u32 atom) {
  return !atom_is_int(atom);
}

}

#endif