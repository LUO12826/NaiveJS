#include "NativeFunction.h"

#include <iostream>
#include "NjsVM.h"

#include <sstream>

namespace njs {

JSValue InternalFunctions::log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  std::ostringstream stream;
  stream << "\033[32m" << "[LOG] "; // green text
  for (int i = 0; i < args.size(); i++) {
    stream << args[i].to_string() << " ";
  }
  stream << std::endl;
  stream << "\033[0m";  // restore normal color

  std::cout << stream.str();
  vm.log_buffer.push_back(stream.str());

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


}