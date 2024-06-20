#ifndef NJS_STACK_FRAME_H
#define NJS_STACK_FRAME_H

#include <cstdint>
#include "njs/basic_types/JSValue.h"

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;

struct JSStackFrame {
  JSStackFrame *prev_frame;
  JSValue function;
  size_t alloc_cnt {0};
  JSValue *buffer;
  JSValue *args_buf;
  JSValue *local_vars;
  JSValue *stack;
  JSValue *sp;
  u32 pc {0};
  // warning: these two values are valid only when this frame is active.
  JSValue **sp_ref;
  u32 *pc_ref;
  JSValue storage[0];

  JSStackFrame* move_to_heap() {
    size_t total_size = sizeof(JSStackFrame) + alloc_cnt * sizeof(JSValue);
    void *data = malloc(total_size);
    JSStackFrame& frame = *(JSStackFrame *)data;
    // copy metadata
    memcpy(data, (void *)this, sizeof(JSStackFrame));
    // copy local buffer
    memcpy(frame.storage, this->buffer, sizeof(JSValue) * this->alloc_cnt);
    
    frame.buffer = frame.storage;
    auto addr_diff = frame.buffer - this->buffer;
    frame.args_buf += addr_diff;
    frame.local_vars += addr_diff;
    frame.stack += addr_diff;
    frame.sp += addr_diff;
    
    return &frame;
  }
};



} // namespace njs

#endif // NJS_STACK_FRAME_H