#include "JSFunction.h"

#include <string>
#include <utility>
#include "conversion.h"
#include "JSPromise.h"
#include "njs/gc/GCHeap.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/common_def.h"

namespace njs {

void JSFunction::add_internal_function_meta(NjsVM& vm) {
  async_fulfill_callback_meta = build_func_meta(async_then_callback, JSPromise::FULFILLED);
  async_reject_callback_meta = build_func_meta(async_then_callback, JSPromise::REJECTED);

  vm.func_meta.emplace_back(async_fulfill_callback_meta);
  vm.func_meta.emplace_back(async_reject_callback_meta);
}

array<JSValue, 2> JSFunction::build_async_then_callback(NjsVM& vm, JSValueRef promise) {
  NOGC;
  auto *resolve_func = vm.new_function(async_fulfill_callback_meta);
  auto *reject_func = vm.new_function(async_reject_callback_meta);
  resolve_func->has_auxiliary_data = true;
  reject_func->has_auxiliary_data = true;
  resolve_func->this_or_auxiliary_data = promise;
  reject_func->this_or_auxiliary_data = promise;

  return {JSValue(resolve_func), JSValue(reject_func)};
}

Completion JSFunction::async_then_callback(vm_func_This_args_flags) {
  assert(func.as_func->has_auxiliary_data);
  assert(func.as_func->this_or_auxiliary_data.as_object->get_class() == CLS_PROMISE);
  assert(args.size() > 0);

  auto promise_state = static_cast<JSPromise::State>(func.as_func->meta->magic);
  JSValueRef promise = func.as_func->this_or_auxiliary_data;
  auto *exec_state = promise.as_Object<JSPromise>()->exec_state;
  assert(exec_state);

  JSValueRef arg = args[0];
  exec_state->stack_frame.sp[0] = arg;
  if (promise_state == JSPromise::REJECTED) {
    exec_state->resume_with_throw = true;
  }
  
  vm.async_resume(promise, exec_state);
  return undefined;
}

JSFunction::JSFunction(NjsVM& vm, u16string_view name, JSFunctionMeta *meta)
    : JSObject(vm, CLS_FUNCTION, vm.function_prototype)
    , name(name)
    , meta(meta)
    , need_arguments_array(meta->need_arguments_array)
    , param_count(meta->param_count)
    , local_var_count(meta->local_var_count)
    , stack_size(meta->stack_size)
    , bytecode_start(meta->bytecode_start)
    , is_constructor(meta->is_constructor)
    , is_arrow_func(meta->is_arrow_func)
    , native_func(meta->native_func) {}

JSFunction::JSFunction(NjsVM& vm, JSFunctionMeta *meta)
    : JSFunction(vm, u"", meta) {}

ResumableFuncState* JSFunction::build_exec_state(NjsVM& vm, JSValueRef This, ArgRef argv) {
  uint64_t args_buf_cnt = std::max((size_t)param_count, argv.size());
  uint64_t frame_buffer_cnt = args_buf_cnt + local_var_count + stack_size;

  uint64_t alloc_size = sizeof(ResumableFuncState) + frame_buffer_cnt * sizeof(JSValue);
  void *data = malloc(alloc_size);

  auto *state = new (data) ResumableFuncState();
  state->This = This;

  JSStackFrame& frame = state->stack_frame;
  frame.function.set_val(this);
  frame.alloc_cnt = frame_buffer_cnt;
  frame.buffer = frame.storage;
  frame.args_buf = frame.buffer;
  frame.local_vars = frame.args_buf + args_buf_cnt;
  frame.stack = frame.local_vars + local_var_count;
  frame.sp = frame.stack - 1;
  frame.pc = bytecode_start;

  for (size_t i = 0; i < argv.size(); i++) {
    set_referenced(argv[i]);
    frame.args_buf[i] = argv[i];
  }
  for (size_t i = argv.size(); i < args_buf_cnt; i++) {
    frame.args_buf[i].set_undefined();
  }

  for (JSValue *val = frame.local_vars; val < frame.stack; val++) {
    val->set_undefined();
  }

  if (need_arguments_array) [[unlikely]] {
    frame.local_vars[0] = prepare_arguments_array(vm, argv);
  }

  return state;
}

bool JSFunction::gc_scan_children(GCHeap& heap) {
  bool child_young = false;
  child_young |= JSObject::gc_scan_children(heap);
  for (auto& var : captured_var) {
    assert(var.is(JSValue::HEAP_VAL));
    child_young |= heap.gc_visit_object(var);
  }
  gc_check_and_visit_object(child_young, this_or_auxiliary_data);
  return child_young;
}

void JSFunction::gc_mark_children() {
  JSObject::gc_mark_children();
  for (auto& var : captured_var) {
    assert(var.is(JSValue::HEAP_VAL));
    gc_mark_object(var.as_GCObject);
  }
  gc_check_and_mark_object(this_or_auxiliary_data);
}

bool JSFunction::gc_has_young_child(GCObject *oldgen_start) {
  if (JSObject::gc_has_young_child(oldgen_start)) return true;
  for (auto& var : captured_var) {
    assert(var.is(JSValue::HEAP_VAL));
    if (var.as_GCObject < oldgen_start) return true;
  }
  gc_check_object_young(this_or_auxiliary_data);
  return false;
}

std::string JSFunction::description() {
  std::string desc;
  desc += "JSFunction ";
  if (meta->is_anonymous) desc += "(anonymous)";
  else {
    desc += "named: ";
    desc += to_u8string(name);
  }

  desc += ", is native: ";
  desc += to_u8string(meta->is_native);
  desc += "  Props: ";
  desc += JSObject::description();

  return desc;
}

std::string JSFunctionMeta::description() const {
  std::string desc;
  if (is_anonymous) desc += "(anonymous)";
  else {
    desc += "name_index: " + std::to_string(name_index);
  }
  desc += ", bytecode_start: " + std::to_string(bytecode_start);

  return desc;
}

}