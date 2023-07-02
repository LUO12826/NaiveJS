#ifndef NJS_UTILS_MACROS_H
#define NJS_UTILS_MACROS_H

#include <cstdlib>

namespace njs {

using u32 = uint32_t;


#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)


}  // namespace njs

#endif  // NJS_UTILS_MACROS_H