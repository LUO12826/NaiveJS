#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>

namespace njs {
    
using u32 = uint32_t;
using atom_t = int64_t;

struct SourceLocation {
  u32 line;
  u32 col;
  u32 u16_char_idx;
};

}

#endif