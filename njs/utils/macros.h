#ifndef NJS_UTILS_MACROS_H
#define NJS_UTILS_MACROS_H

#include <cstdlib>

namespace njs {

using u32 = uint32_t;

#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define force_inline inline __attribute__((always_inline))
#else
#define js_likely(x) (x)
#define js_unlikely(x) (x)
#define force_inline inline
#endif

#define object_class(x) ((x).as_object->get_class())
#define set_referenced(x) if ((x).needs_gc()) { (x).as_GCObject->ref_count_inc(); }
#define gc_write_barrier(x) vm.heap.write_barrier(this, x);

#define gc_check_and_visit_object(res, o) if ((o).needs_gc()) { (res) |= heap.gc_visit_object((o).as_GCObject); }
#define gc_check_and_mark_object(o) if ((o).needs_gc()) { gc_mark_object((o).as_GCObject); }
#define gc_check_object_young(o) if ((o).needs_gc() && (o).as_GCObject < oldgen_start) { return true; }

#define NOGC NoGC nogc(vm);
#define HANDLE_COLLECTOR GCHandleCollector collector(vm);
#define gc_handle_add(o) collector.collect(o);

/// try something that produces `Completion`, return `CompThrow` if get an error.
#define TRYCC(expression)                                                                     \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_throw()) [[unlikely]] {                                           \
          return _temp_result;                                                                \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

/// try something that produces `Completion` or `ErrorOr<>`, return `Error` if get an error.
#define TRY_ERR(expression)                                                                   \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_error()) [[unlikely]] {                                           \
          return _temp_result.get_error();                                                    \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

/// try something that produces `Completion` or `ErrorOr<>`, return that `Completion` if get an error.
#define TRY_COMP(expression)                                                                  \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_error()) [[unlikely]] {                                           \
          return CompThrow(_temp_result.get_error());                                         \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

}  // namespace njs

#endif  // NJS_UTILS_MACROS_H