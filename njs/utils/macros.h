#ifndef NJS_UTILS_MACROS_H
#define NJS_UTILS_MACROS_H

#include <cstdlib>

namespace njs {

using u32 = uint32_t;

#define object_class(x) ((x).as_object->get_class())

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/// try something that produces `ErrorOr<>`, return `CompThrow` if get an error.
#define TRYCC(expression)                                                                     \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_throw()) [[unlikely]] {                                           \
          return _temp_result;                                                                \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

/// try something that produces `ErrorOr<>`, return `Error` if get an error.
#define TRY_ERR(expression)                                                                   \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_error()) [[unlikely]] {                                           \
          return _temp_result.get_error();                                                    \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

/// try something that produces `Completion`, return that `Completion` if get an error.
#define TRY_COMP(expression)                                                                  \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_error()) [[unlikely]] {                                           \
          return CompThrow(_temp_result.get_error());                                         \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

/// try something that produces `Completion`, return the error from the completion if get an error.
//#define TRY_ERR(expression)                                                              \
//    ({                                                                                        \
//        Completion _temp_result = (expression);                                               \
//        if (_temp_result.is_throw()) [[unlikely]] {                                           \
//          return _temp_result.get_value();                                                    \
//        }                                                                                     \
//        _temp_result.get_value();                                                             \
//    })

}  // namespace njs

#endif  // NJS_UTILS_MACROS_H