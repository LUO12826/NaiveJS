#ifndef NJS_UTILS_MACROS_H
#define NJS_UTILS_MACROS_H

#include <cstdlib>

namespace njs {

using u32 = uint32_t;


#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define TRY(expression)                                                                       \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_error()) [[unlikely]] {                                           \
          return Completion::with_throw(_temp_result.get_error());                            \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

#define TRY_ERR(expression)                                                                   \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_error()) [[unlikely]] {                                           \
          return _temp_result.get_error();                                                    \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

#define TRY_COMP(expression)                                                                  \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_throw()) [[unlikely]] {                                           \
          return _temp_result;                                                                \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })


}  // namespace njs

#endif  // NJS_UTILS_MACROS_H