#include "NativeFunction.h"

#include <iostream>
#include "NjsVM.h"

#include <sstream>

namespace njs {

void add_basic_functions(NjsVM& vm) {

}

JSValue log(NjsVM& vm, ArrayRef<JSValue> args) {
  std::ostringstream stream;
  stream << "\033[32m" << "[LOG] "; // green text
  for (int i = 0; i < args.size(); i++) {
    stream << args[i].to_string() << ", ";
  }
  stream << std::endl;
  stream << "\033[0m";  // restore normal color

  vm.log_buffer.push_back(stream.str());

  return JSValue::undefined;
}

}