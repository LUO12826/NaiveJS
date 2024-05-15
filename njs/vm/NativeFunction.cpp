#include "NativeFunction.h"

#include "NjsVM.h"
#include "njs/include/httplib.h"

namespace njs {

Completion NativeFunction::log(vm_func_This_args_flags) {
  std::string output = "\033[32m[LOG] ";

  for (int i = 0; i < args.size(); i++) {
    output += args[i].to_string(vm);
    output += " ";
  }

  output += "\n\033[0m";
  printf("%s", output.c_str());
  vm.log_buffer.push_back(std::move(output));

  return undefined;
}

Completion NativeFunction::debug_log(vm_func_This_args_flags) {
  std::string output = "\033[32m[LOG] ";

  for (int i = 0; i < args.size(); i++) {
    output += args[i].to_string(vm);
    output += " ";
  }

  output += "\n\033[0m";
  printf("%s", output.c_str());

  return undefined;
}

Completion NativeFunction::js_gc(vm_func_This_args_flags) {
  vm.heap.gc();
  return undefined;
}

Completion NativeFunction::set_timeout(vm_func_This_args_flags) {
  assert(args.size() >= 2);
  assert(args[0].is(JSValue::FUNCTION));
  assert(args[1].is(JSValue::NUM_FLOAT));
  size_t id = vm.runloop.add_timer(args[0].val.as_func, (size_t)args[1].val.as_f64, false);
  return JSValue(double(id));
}

Completion NativeFunction::set_interval(vm_func_This_args_flags) {
  assert(args.size() >= 2);
  assert(args[0].is(JSValue::FUNCTION));
  assert(args[1].is(JSValue::NUM_FLOAT));
  size_t id = vm.runloop.add_timer(args[0].val.as_func, (size_t)args[1].val.as_f64, true);
  return JSValue(double(id));
}

Completion NativeFunction::clear_interval(vm_func_This_args_flags) {
  assert(args.size() >= 1);
  assert(args[0].is(JSValue::NUM_FLOAT));
  vm.runloop.remove_timer(size_t(args[0].val.as_f64));
  return undefined;
}

Completion NativeFunction::clear_timeout(vm_func_This_args_flags) {
  assert(args.size() >= 1);
  assert(args[0].is(JSValue::NUM_FLOAT));
  vm.runloop.remove_timer(size_t(args[0].val.as_f64));
  return undefined;
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

Completion NativeFunction::fetch(vm_func_This_args_flags) {
  assert(args.size() >= 2);
  assert(args[0].is(JSValue::STRING));
  assert(args[1].is(JSValue::FUNCTION));

  std::string url = to_u8string(args[0].val.as_prim_string->str);
  JSTask *task = vm.runloop.add_task(args[1].val.as_func);

  vm.runloop.get_thread_pool().push_task([&vm, task] (const std::string& url) {
    std::string host, path;
    separate_host_and_path(url, host, path);

    httplib::Client cli(host);

    if (auto res = cli.Get(path)) {
      task->args.emplace_back((double)res->status);
      task->args.emplace_back(vm.heap.new_object<PrimitiveString>(to_u16string(res->body)));
    } else {
      auto err = res.error();
      std::cout << "HTTP error: " << httplib::to_string(err) << '\n';
    }

    vm.runloop.post_task(task);

  }, std::move(url));

  return undefined;
}

Completion NativeFunction::json_stringify(vm_func_This_args_flags) {
  assert(args.size() >= 1);
  u16string json_string;
  args[0].to_json(json_string, vm);
  return vm.new_primitive_string(std::move(json_string));
}

}