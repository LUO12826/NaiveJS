#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>

namespace njs {
    
using u32 = uint32_t;
using i64 = int64_t;


struct SourceLoc {
  u32 line {0};
  u32 col {0};
  u32 char_idx {0};
};

}

#endif