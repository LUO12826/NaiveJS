#include "NativeFunction.h"

#include "NjsVM.h"
#include "njs/include/httplib.h"

namespace njs {

Completion InternalFunctions::log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  std::string output = "\033[32m[LOG] ";

  for (int i = 0; i < args.size(); i++) {
    output += args[i].to_string(vm);
    output += " ";
  }

  output += "\n\033[0m";
  printf("%s", output.c_str());
  vm.log_buffer.push_back(std::move(output));

  return JSValue::undefined;
}

Completion InternalFunctions::debug_log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  std::string output = "\033[32m[LOG] ";

  for (int i = 0; i < args.size(); i++) {
    output += args[i].to_string(vm);
    output += " ";
  }

  output += "\n\033[0m";
  printf("%s", output.c_str());

  return JSValue::undefined;
}

Completion InternalFunctions::js_gc(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  vm.heap.gc();
  return JSValue::undefined;
}

Completion InternalFunctions::set_timeout(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 2);
  assert(args[0].is(JSValue::FUNCTION));
  assert(args[1].is(JSValue::NUM_FLOAT));
  size_t id = vm.runloop.add_timer(args[0].val.as_function, (size_t)args[1].val.as_float64, false);
  return JSValue(double(id));
}

Completion InternalFunctions::set_interval(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 2);
  assert(args[0].is(JSValue::FUNCTION));
  assert(args[1].is(JSValue::NUM_FLOAT));
  size_t id = vm.runloop.add_timer(args[0].val.as_function, (size_t)args[1].val.as_float64, true);
  return JSValue(double(id));
}

Completion InternalFunctions::clear_interval(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 1);
  assert(args[0].is(JSValue::NUM_FLOAT));
  vm.runloop.remove_timer(size_t(args[0].val.as_float64));
  return JSValue::undefined;
}

Completion InternalFunctions::clear_timeout(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 1);
  assert(args[0].is(JSValue::NUM_FLOAT));
  vm.runloop.remove_timer(size_t(args[0].val.as_float64));
  return JSValue::undefined;
}

void separate_host_and_path(const std::string& url, std::string& host, std::string& path) {
  size_t start_pos = url.find("://");
  if (start_pos != std::string::npos) {
    start_pos += 3; // Move past the "://"
  } else {
    start_pos = 0;
  }

  size_t host_end_pos = url.find('/', start_pos);
  if (host_end_pos != std::string::npos) {
    host = url.substr(0, host_end_pos);
    path = url.substr(host_end_pos);
  } else {
    host = url.substr(start_pos);
    path = "/";
  }
}

Completion InternalFunctions::fetch(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 2);
  assert(args[0].is(JSValue::STRING));
  assert(args[1].is(JSValue::FUNCTION));

  std::string url = to_u8string(args[0].val.as_primitive_string->str);
  JSTask *task = vm.runloop.add_task(args[1].val.as_function);

  vm.runloop.get_thread_pool().push_task([&vm, task] (const std::string& url) {
    std::string host, path;
    separate_host_and_path(url, host, path);

    httplib::Client cli(host);

    if (auto res = cli.Get(path)) {
      task->args.emplace_back((double)res->status);
      task->args.emplace_back(new PrimitiveString(to_u16string(res->body)));
    } else {
      auto err = res.error();
      std::cout << "HTTP error: " << httplib::to_string(err) << '\n';
    }

    vm.runloop.post_task(task);

  }, std::move(url));

  return JSValue::undefined;
}

Completion InternalFunctions::json_stringify(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  assert(args.size() >= 1);
  u16string json_string;
  args[0].to_json(json_string, vm);
  return JSValue(new PrimitiveString(std::move(json_string)));
}

u16string build_trace_str(NjsVM& vm) {
  std::vector<NjsVM::StackTraceItem> trace = vm.capture_stack_trace();

  u16string trace_str;
  for (auto& tr : trace) {
    trace_str += u"  ";
    trace_str += tr.func_name;
    if (not tr.is_native) {
      trace_str += u"  @ line ";
      trace_str += to_u16string(std::to_string(tr.source_line));
    }
    else {
      trace_str += u"  (native)";
    }
    trace_str += u"\n";
  }
  return trace_str;
}

Completion InternalFunctions::error_ctor(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  auto *err_obj = vm.new_object(ObjectClass::CLS_ERROR);
  if (args.size() > 0 && args[0].is_string_type()) {
    // only supports primitive string now.
    assert(args[0].is(JSValue::STRING));
    err_obj->add_prop(vm, u"message", JSValue(args[0].val.as_primitive_string));
  }

  u16string trace_str = build_trace_str(vm);
  err_obj->add_prop(vm, u"stack", JSValue(new PrimitiveString(std::move(trace_str))));

  return JSValue(err_obj);
}

JSValue InternalFunctions::error_build_internal(NjsVM& vm, const u16string& msg) {
  auto *err_obj = vm.new_object(ObjectClass::CLS_ERROR);
  err_obj->add_prop(vm, u"message", JSValue(new PrimitiveString(msg)));

  u16string trace_str = build_trace_str(vm);
  err_obj->add_prop(vm, u"stack", JSValue(new PrimitiveString(std::move(trace_str))));

  return JSValue(err_obj);
}

Completion InternalFunctions::test_throw_err(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
  return Completion::with_throw(error_build_internal(vm, u"msg"));
}

}