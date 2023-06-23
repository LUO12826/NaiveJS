#ifndef NJS_UTILS_MACROS_H
#define NJS_UTILS_MACROS_H

#include <stdlib.h>

namespace njs {

using u32 = uint32_t;


#define PTR(ptr, offset) \
  (reinterpret_cast<char*>(ptr) + offset)

#define TYPED_PTR(ptr, offset, type) \
  reinterpret_cast<type*>((reinterpret_cast<char*>(ptr) + offset))

#define HEAP_PTR(ptr, offset) \
  reinterpret_cast<HeapObject**>(PTR(ptr, offset))

#define SET_HANDLE_VALUE(ptr, offset, handle, type) \
  *reinterpret_cast<type**>(PTR(ptr, offset)) = handle.val()

#define READ_HANDLE_VALUE(ptr, offset, type) \
  Handle<type>(*reinterpret_cast<type**>(PTR(ptr, offset)))
// Read non pointer value, e.g. bool, Type.
#define READ_VALUE(ptr, offset, type) \
  *reinterpret_cast<type*>(PTR(ptr, offset))
// Set Method
#define SET_VALUE(ptr, offset, val, type) \
  *reinterpret_cast<type*>(PTR(ptr, offset)) = val

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#ifdef TEST
#define ASSERT(x) assert(x)
#define TEST_LOG(x...) \
  if (unlikely(debug::Debugger::On())) \
    debug::PrintSource(x)
#else
#define ASSERT(x)
#define TEST_LOG(x...)
#endif

}  // namespace njs

#endif  // NJS_UTILS_MACROS_H