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

  vm.log_buffer.push_back(stream.str());

  return JSValue::undefined;
}

JSValue InternalFunctions::js_gc(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  vm.heap.gc();
  return JSValue::undefined;
}

}