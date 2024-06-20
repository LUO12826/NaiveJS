#ifndef NJS_COMMON_DEF_H
#define NJS_COMMON_DEF_H

#include <cstdint>

namespace njs {

constexpr uint32_t frame_meta_size {0};
constexpr u32 CHAR_SIZE {sizeof(char16_t)};

struct JSValue;
using JSValueRef = JSValue const&;

struct CallFlags {
  bool constructor : 1 {false};
  bool copy_args : 1 {false};
  bool buffer_on_heap : 1 {false};
  bool generator : 1 {false};
  bool this_is_new_target : 1 {false};
  int magic : 27;
};

#define JS_NATIVE_FUNC_PARAMS NjsVM& vm, JSValueRef func, JSValueRef This, Span<JSValue> args, CallFlags flags
#define vm_func_This_args_flags NjsVM& vm, JSValueRef func, JSValueRef This, Span<JSValue> args, CallFlags flags
#define JS_NATIVE_FUNC_ARGS vm, func, This, args, flags

}


#endif //NJS_COMMON_DEF_H
