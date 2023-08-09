#ifndef NJS_CATCH_TABLE_ENTRY
#define NJS_CATCH_TABLE_ENTRY

#include <cstdint>

namespace njs {

using u32 = uint32_t;

struct CatchTableEntry {
  u32 start_pos;
  u32 end_pos;
  u32 goto_pos;

  CatchTableEntry(u32 start, u32 end, u32 goto_pos)
    : start_pos(start), end_pos(end), goto_pos(goto_pos) {}

  bool pos_in_range(u32 pos) {
    return pos >= start_pos && pos <= end_pos;
  }
};

}

#endif