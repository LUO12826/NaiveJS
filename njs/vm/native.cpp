#include "native.h"

#include <random>
#include "NjsVM.h"
#include "JSONParser.h"
#include "njs/basic_types/conversion.h"
#include "njs/include/httplib.h"

namespace njs::native {

Completion misc::log(vm_func_This_args_flags) {
  std::string output = "\033[32m[LOG] ";

  for (auto const& arg : args) {
    output += arg.to_string(vm);
    output += " ";
  }

  output += "\n\033[0m";
  printf("%s", output.c_str());
  vm.log_buffer.push_back(std::move(output));

  return undefined;
}

Completion misc::debug_log(vm_func_This_args_flags) {
  std::string output;

  for (auto const& arg : args) {
    output += arg.to_string(vm);
    output += " ";
  }

  printf("%s\n", output.c_str());

  return undefined;
}

Completion misc::debug_trap(vm_func_This_args_flags) {
  if (not args.empty() && args[0].bool_value()) {
    return vm.throw_error(JS_INTERNAL_ERROR, u"Trap");
  } else {
    return undefined;
  }
}

Completion misc::dummy(vm_func_This_args_flags) {
  return undefined;
}

Completion misc::_test(vm_func_This_args_flags) {
  HANDLE_COLLECTOR;
  JSValue obj(vm.new_object());
  gc_handle_add(obj);
  printf("%p\n", obj.as_object);
  vm.heap.gc();
  printf("%p\n", obj.as_object);
  return undefined;
}

Completion misc::js_gc(vm_func_This_args_flags) {
  vm.heap.gc();
  return undefined;
}

Completion misc::set_timer(NjsVM&vm, ArgRef args, bool repeat) {
  if (args.empty() || not args[0].is_function()) {
    return vm.throw_error(JS_TYPE_ERROR, u"The first argument must be a function");
  }

  size_t timeout = 0;
  if (args.size() > 1) {
    timeout = (size_t)TRY_COMP(js_to_number(vm, args[1]));
  }
  size_t id = vm.runloop.add_timer(args[0].as_func, timeout, repeat);
  return JSFloat(id);
}


Completion misc::setTimeout(vm_func_This_args_flags) {
  return set_timer(vm, args, false);
}

Completion misc::setInterval(vm_func_This_args_flags) {
  return set_timer(vm, args, true);
}

Completion misc::clearInterval(vm_func_This_args_flags) {
  if (not args.empty()) [[likely]] {
    double val = TRY_COMP(js_to_number(vm, args[0]));
    vm.runloop.remove_timer(size_t(val));
  }
  return undefined;
}

Completion misc::clearTimeout(vm_func_This_args_flags) {
  if (not args.empty()) [[likely]] {
    double val = TRY_COMP(js_to_number(vm, args[0]));
    vm.runloop.remove_timer(size_t(val));
  }
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

Completion misc::fetch(vm_func_This_args_flags) {
  assert(args.size() >= 2);
  assert(args[0].is(JSValue::STRING));
  assert(args[1].is(JSValue::FUNCTION));

  std::string url = to_u8string(args[0].as_prim_string->view());
  JSTask *task = vm.runloop.register_task(args[1].as_func);

  vm.runloop.get_thread_pool().push_task([&vm, task] (const std::string& url) {
    std::string host, path;
    separate_host_and_path(url, host, path);

    httplib::Client cli(host);

    if (auto res = cli.Get(path)) {
      task->args.emplace_back((double)res->status);
      task->args.emplace_back(vm.new_primitive_string(to_u16string(res->body)));
    } else {
      auto err = res.error();
      std::cout << "HTTP error: " << httplib::to_string(err) << '\n';
    }

    vm.runloop.exec_task(task);

  }, std::move(url));

  return undefined;
}

Completion misc::json_parse(vm_func_This_args_flags) {
  JSValue arg;
  if (!args.empty()) [[likely]] {
    arg = args[0];
  }
  JSValue arg_str = TRYCC(js_to_string(vm, arg));
  u16string json_str(arg_str.as_prim_string->view());

  return JSONParser(vm, json_str).parse();
}

Completion misc::json_stringify(vm_func_This_args_flags) {
  NOGC;
  JSValue arg;
  if (!args.empty()) [[likely]] {
    arg = args[0];
  }
  u16string json_string;
  arg.to_json(json_string, vm);
  return vm.new_primitive_string(json_string);
}

Completion misc::isFinite(vm_func_This_args_flags) {
  if (args.empty()) return JSValue(false);
  double val;
  if (args[0].is_float64()) [[likely]] {
    val = args[0].as_f64;
  } else {
    val = TRY_COMP(js_to_number(vm, args[0]));
  }

  return JSValue(!std::isinf(val) && !std::isnan(val));
}

Completion misc::parseFloat(vm_func_This_args_flags) {
  if (args.empty()) return JSValue(NAN);
  PrimitiveString *str = TRYCC(js_to_string(vm, args[0])).as_prim_string;
  double val = parse_double(str->view());

  return JSValue(val);
}

Completion misc::parseInt(vm_func_This_args_flags) {
  if (args.empty()) return JSValue(NAN);
  PrimitiveString *str = TRYCC(js_to_string(vm, args[0])).as_prim_string;
  double val = parse_int(str->view());

  return JSValue(val);
}

Completion Math::min(vm_func_This_args_flags) {
  assert(args.size() == 2 && args[0].is_float64() && args[1].is_float64());
  return JSValue(std::fmin(args[0].as_f64, args[1].as_f64));
}

Completion Math::max(vm_func_This_args_flags) {
  assert(args.size() == 2 && args[0].is_float64() && args[1].is_float64());
  return JSValue(std::fmax(args[0].as_f64, args[1].as_f64));
}

Completion Math::floor(vm_func_This_args_flags) {
  assert(args.size() == 1 && args[0].is_float64());
  return JSValue(std::floor(args[0].as_f64));
}

Completion Math::random(vm_func_This_args_flags) {
  static std::uniform_real_distribution<> dis {0.0, 1.0};
  return JSValue(dis(vm.random_engine));
}

}
