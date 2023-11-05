#ifndef NJS_RUNTIME_H
#define NJS_RUNTIME_H

namespace njs {

enum class CallResult {
  UNFINISHED,
  DONE_NORMAL,
  DONE_ERROR,
};

} // namespace njs

#endif //NJS_RUNTIME_H
