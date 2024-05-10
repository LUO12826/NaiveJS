#ifndef NJS_COMMON_DEF_H
#define NJS_COMMON_DEF_H

#include <cstdint>

constexpr uint32_t frame_meta_size {0};

struct CallFlags {
  bool constructor : 1 {false};
  bool copy_args : 1 {false};
  bool buffer_on_heap : 1 {false};
  bool generator : 1 {false};
  bool this_is_new_target : 1 {false};
};

#define JS_NATIVE_FUNC_PARAMS NjsVM& vm, JSFunction& func, JSValue This, ArrayRef<JSValue> args, CallFlags flags
#define vm_func_This_args_flags NjsVM& vm, JSFunction& func, JSValue This, ArrayRef<JSValue> args, CallFlags flags

#endif //NJS_COMMON_DEF_H
