#include "NativeFunction.h"

#include <iostream>
#include "NjsVM.h"

namespace njs {

void add_basic_functions(NjsVM& vm) {

}

JSValue log(NjsVM& vm, ArrayRef<JSValue> args) {

  for (int i = 0; i < args.size(); i++) {
    std::cout << args[i].to_string() << ", ";
  }
  std::cout << std::endl;

  return JSValue::undefined;
}

}