#include "NativeFunction.h"

#include "NjsVM.h"

namespace njs {

JSValue InternalFunctions::log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  std::string output = "\033[32m[LOG] ";

  for (int i = 0; i < args.size(); i++) {
    output += args[i].to_string();
    output += " ";
  }

  output += "\n\033[0m";
  printf("%s", output.c_str());

  vm.log_buffer.push_back(std::move(output));
  return JSValue::undefined;
}

JSValue InternalFunctions::js_gc(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  vm.heap.gc();
  return JSValue::undefined;
}

JSValue InternalFunctions::set_timeout(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 2);
  assert(args[0].tag_is(JSValue::FUNCTION));
  assert(args[1].tag_is(JSValue::NUM_FLOAT));
  size_t id = vm.runloop.add_timer(args[0].val.as_function, (size_t)args[1].val.as_float64, false);
  return JSValue(double(id));
}

JSValue InternalFunctions::set_interval(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 2);
  assert(args[0].tag_is(JSValue::FUNCTION));
  assert(args[1].tag_is(JSValue::NUM_FLOAT));
  size_t id = vm.runloop.add_timer(args[0].val.as_function, (size_t)args[1].val.as_float64, true);
  return JSValue(double(id));
}

JSValue InternalFunctions::clear_interval(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 1);
  assert(args[0].tag_is(JSValue::NUM_FLOAT));
  vm.runloop.remove_timer(size_t(args[0].val.as_float64));
  return JSValue::undefined;
}

JSValue InternalFunctions::clear_timeout(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 1);
  assert(args[0].tag_is(JSValue::NUM_FLOAT));
  vm.runloop.remove_timer(size_t(args[0].val.as_float64));
  return JSValue::undefined;
}

JSValue InternalFunctions::fetch(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 2);
  assert(args[0].tag_is(JSValue::STRING));
  assert(args[1].tag_is(JSValue::FUNCTION));


}


}