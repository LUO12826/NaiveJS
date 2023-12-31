#ifndef NJS_CATCH_TABLE_ENTRY
#define NJS_CATCH_TABLE_ENTRY

#include <cstdint>
#include <sstream>
#include <iomanip>

namespace njs {

using u32 = uint32_t;

struct CatchTableEntry {
  u32 start_pos;
  u32 end_pos;
  u32 goto_pos;

  u32 local_var_begin;
  u32 local_var_end;

  CatchTableEntry(u32 start, u32 end, u32 goto_pos)
    : start_pos(start), end_pos(end), goto_pos(goto_pos) {}

  bool range_include(u32 pos) const {
    return pos >= start_pos && pos <= end_pos;
  }

  std::string description() const {
    std::ostringstream ss;
    ss << "start: " << std::setw(4) << start_pos << " end: " << std::setw(4) << end_pos
       << " goto: " << std::setw(4) << goto_pos << " var start: " << std::setw(4) << local_var_begin
       << " var end: " << std::setw(4) << local_var_end;

    return ss.str();
  }
};

}

#endif