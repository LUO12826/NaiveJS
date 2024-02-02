
#include "NativeFunction.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/JSBoolean.h"
#include "njs/basic_types/JSNumber.h"
#include "njs/basic_types/JSString.h"

namespace njs {

Completion InternalFunctions::Object_ctor(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  if (args.size() == 0 || args[0].is_null() || args[0].is_undefined()) {
    return JSValue(vm.new_object());
  }
  JSValue arg = args[0];

  if (arg.is_object()) return arg;

  JSObject *obj;
  switch (arg.tag) {
    case JSValue::BOOLEAN:
      obj = vm.heap.new_object<JSBoolean>(vm, arg.val.as_bool);
      break;
    case JSValue::NUM_INT:
      obj = vm.heap.new_object<JSNumber>(vm, arg.val.as_int64);
      break;
    case JSValue::NUM_FLOAT:
      obj = vm.heap.new_object<JSNumber>(vm, arg.val.as_float64);
      break;
    case JSValue::STRING:
      obj = vm.heap.new_object<JSString>(vm, *arg.val.as_primitive_string);
      break;
    default:
      assert(false);
  }

  return JSValue(obj);
}

Completion InternalFunctions::Error_ctor(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  auto *err_obj = vm.new_object(ObjectClass::CLS_ERROR);
  if (args.size() > 0 && args[0].is_string_type()) {
    // only supports primitive string now.
    assert(args[0].is(JSValue::STRING));
    err_obj->add_prop(vm, u"message", JSValue(args[0].val.as_primitive_string));
  }

  u16string trace_str = InternalFunctions::build_trace_str(vm);
  err_obj->add_prop(vm, u"stack", JSValue(new PrimitiveString(std::move(trace_str))));

  return JSValue(err_obj);
}

}