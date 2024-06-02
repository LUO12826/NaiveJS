#ifndef NJS_LRE_HELPER
#define NJS_LRE_HELPER

#include <optional>
#include <utility>
#include <string>
#include <cstdint>
#include <cassert>
extern "C" {
#include "libregexp.h"
}


namespace njs {

using std::pair;
using std::optional;
using std::u16string;

inline optional<int> str_to_regexp_flags(const u16string& flags) {
  int re_flags = 0;
  for (char16_t flag : flags) {
    int mask;
    switch(flag) {
      case 'd':
        mask = LRE_FLAG_INDICES;
        break;
      case 'g':
        mask = LRE_FLAG_GLOBAL;
        break;
      case 'i':
        mask = LRE_FLAG_IGNORECASE;
        break;
      case 'm':
        mask = LRE_FLAG_MULTILINE;
        break;
      case 's':
        mask = LRE_FLAG_DOTALL;
        break;
      case 'u':
        mask = LRE_FLAG_UNICODE;
        break;
      case 'y':
        mask = LRE_FLAG_STICKY;
        break;
      default:
        goto bad_flags;
    }
    if ((re_flags & mask) != 0) {
    bad_flags:
      return std::nullopt;
    }
    re_flags |= mask;
  }
  return re_flags;
}

inline u16string regexp_flags_to_str(int flags) {
  u16string flag_str;
  if (flags & LRE_FLAG_GLOBAL) flag_str += u'g';
  if (flags & LRE_FLAG_IGNORECASE) flag_str += u'i';
  if (flags & LRE_FLAG_MULTILINE) flag_str += u'm';
  if (flags & LRE_FLAG_DOTALL) flag_str += u's';
  if (flags & LRE_FLAG_UNICODE) flag_str += u'u';
  if (flags & LRE_FLAG_STICKY) flag_str += u'y';
  if (flags & LRE_FLAG_INDICES) flag_str += u'd';

  return flag_str;
}

class LREWrapper {
 public:
  LREWrapper(uint8_t *bytecode, const u16string& input)
    : bytecode(bytecode)
    , input(input)
    , input_buf((uint8_t*)input.c_str())
    , input_len(input.size())
  {
    capture_cnt = lre_get_capture_count(bytecode);
    if (capture_cnt > 0) {
      capture = new uint8_t*[capture_cnt * 2];
    }
  }

  ~LREWrapper() {
    delete[] capture;
  }

  int get_capture_cnt() {
    return capture_cnt;
  }

  const char *get_groupnames() {
    return lre_get_groupnames(bytecode);
  }

  int exec(int start_index) {
    executed = true;
    return lre_exec(capture, bytecode, input_buf, start_index, input_len, is_wchar, nullptr);
  }

  size_t get_matched_end() {
    assert(executed);
    return (capture[1] - input_buf) >> is_wchar;
  }

  size_t get_matched_start() {
    assert(executed);
    return (capture[0] - input_buf) >> is_wchar;
  }


  pair<size_t, size_t> capture_group_get_start_end(int index) {
    assert(executed);
    size_t start = (capture[2 * index] - input_buf) >> is_wchar;
    size_t end = (capture[2 * index + 1] - input_buf) >> is_wchar;
    return {start, end};
  }

  u16string get_group_match_result(int index) {
    auto [start, end] = capture_group_get_start_end(index);
    return input.substr(start, end - start);
  }

  bool captured_at_group_index(int index) {
    assert(executed);
    return capture[2 * index] != nullptr && capture[2 * index + 1] != nullptr;
  }

 private:
  uint8_t *bytecode {nullptr};

  const u16string& input;
  uint8_t *input_buf {nullptr};
  int input_len {0};
  bool is_wchar {true};

  // capture array:
  // each element is a pointer pointing to a char position (in the input string).
  // capture[0] and [1] are for the substring matched by the entire regexp.
  // The substring is given by [ capture[0], capture[1] ).
  // capture[2] and [3] are for the substring matched by the first group.
  // and this substring is given by [ capture[2], capture[3] ).
  // ...
  uint8_t **capture {nullptr};
  int capture_cnt {0};

  bool executed {false};
};

}

#endif // NJS_LRE_HELPER